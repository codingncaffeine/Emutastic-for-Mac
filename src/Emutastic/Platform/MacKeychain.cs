using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Emutastic.Platform
{
    /// <summary>
    /// macOS backend for <see cref="SecretStore"/>: generic-password items in the user's login
    /// Keychain through Security.framework's SecItem API — where gh and git-credential-osxkeychain
    /// keep GitHub tokens on macOS. The Linux build uses libsecret for the same job.
    ///
    /// Items are keyed service = <see cref="Service"/>, account = "&lt;credential&gt;@&lt;data root&gt;",
    /// so a portable copy never reads or replaces the installed app's sign-in (libsecret's
    /// <c>profile</c> attribute). The Keychain grants access to the app whose code signature
    /// created the item, which is why a stable signing identity matters: a rebuild or in-app
    /// update signed with the same identity reads its items without a prompt.
    ///
    /// Calls can block on a Keychain unlock or access prompt, so never call these on the UI
    /// thread. Failures come back as false with a reason, never as an exception.
    /// </summary>
    internal static class MacKeychain
    {
        private const string SecurityLib = "/System/Library/Frameworks/Security.framework/Security";
        private const string CoreFoundationLib = "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation";

        private const string Service = "io.github.codingncaffeine.Emutastic";

        private const int errSecSuccess = 0;
        private const int errSecDuplicateItem = -25299;
        private const int errSecItemNotFound = -25300;
        private const uint kCFStringEncodingUTF8 = 0x08000100;

        private static readonly object _gate = new();
        private static bool _loaded;
        private static string? _unavailable;

        // CFStringRef / CFBooleanRef constants, read from the frameworks' exported variables.
        private static IntPtr kSecClass, kSecClassGenericPassword, kSecAttrService, kSecAttrAccount,
                              kSecAttrLabel, kSecValueData, kSecReturnData, kSecMatchLimit,
                              kSecMatchLimitOne, kCFBooleanTrue;
        // Addresses of the callback structs themselves (CFDictionaryCreate takes pointers to them).
        private static IntPtr _keyCallBacks, _valueCallBacks;

        [DllImport(SecurityLib)] private static extern int SecItemAdd(IntPtr attributes, IntPtr result);
        [DllImport(SecurityLib)] private static extern int SecItemUpdate(IntPtr query, IntPtr attributesToUpdate);
        [DllImport(SecurityLib)] private static extern int SecItemCopyMatching(IntPtr query, out IntPtr result);
        [DllImport(SecurityLib)] private static extern int SecItemDelete(IntPtr query);
        [DllImport(SecurityLib)] private static extern IntPtr SecCopyErrorMessageString(int status, IntPtr reserved);

        [DllImport(CoreFoundationLib)] private static extern IntPtr CFStringCreateWithCString(IntPtr alloc, byte[] cStr, uint encoding);
        [DllImport(CoreFoundationLib)] private static extern bool CFStringGetCString(IntPtr str, byte[] buffer, nint bufferSize, uint encoding);
        [DllImport(CoreFoundationLib)] private static extern IntPtr CFDataCreate(IntPtr alloc, byte[] bytes, nint length);
        [DllImport(CoreFoundationLib)] private static extern nint CFDataGetLength(IntPtr data);
        [DllImport(CoreFoundationLib)] private static extern IntPtr CFDataGetBytePtr(IntPtr data);
        [DllImport(CoreFoundationLib)] private static extern IntPtr CFDictionaryCreate(IntPtr alloc, IntPtr[] keys, IntPtr[] values,
            nint numValues, IntPtr keyCallBacks, IntPtr valueCallBacks);
        [DllImport(CoreFoundationLib)] private static extern void CFRelease(IntPtr cf);

        public static bool TryStore(string credential, string secret, string label, out string? error)
        {
            lock (_gate)
            {
                if (!EnsureLoaded(out error)) return false;
                byte[] bytes = Encoding.UTF8.GetBytes(secret);
                using var cf = new CfScope();
                IntPtr query = Query(cf, credential);
                IntPtr update = cf.Dict(new[] { kSecValueData, kSecAttrLabel },
                                        new[] { cf.Data(bytes), cf.Str(label) });
                int status = SecItemUpdate(query, update);
                if (status == errSecItemNotFound)
                {
                    IntPtr add = cf.Dict(
                        new[] { kSecClass, kSecAttrService, kSecAttrAccount, kSecAttrLabel, kSecValueData },
                        new[] { kSecClassGenericPassword, cf.Str(Service), cf.Str(Account(credential)), cf.Str(label), cf.Data(bytes) });
                    status = SecItemAdd(add, IntPtr.Zero);
                    if (status == errSecDuplicateItem)   // raced another writer; the update now lands
                        status = SecItemUpdate(query, update);
                }
                Array.Clear(bytes);
                error = status == errSecSuccess ? null : Describe(status);
                return error == null;
            }
        }

        public static bool TryLookup(string credential, out string? secret, out string? error)
        {
            secret = null;
            lock (_gate)
            {
                if (!EnsureLoaded(out error)) return false;
                using var cf = new CfScope();
                IntPtr query = cf.Dict(
                    new[] { kSecClass, kSecAttrService, kSecAttrAccount, kSecReturnData, kSecMatchLimit },
                    new[] { kSecClassGenericPassword, cf.Str(Service), cf.Str(Account(credential)), kCFBooleanTrue, kSecMatchLimitOne });
                int status = SecItemCopyMatching(query, out IntPtr data);
                if (status == errSecItemNotFound) { error = null; return true; }
                if (status != errSecSuccess) { error = Describe(status); return false; }
                try
                {
                    int len = checked((int)CFDataGetLength(data));
                    var bytes = new byte[len];
                    if (len > 0) Marshal.Copy(CFDataGetBytePtr(data), bytes, 0, len);
                    secret = Encoding.UTF8.GetString(bytes);
                    Array.Clear(bytes);
                    error = null;
                    return true;
                }
                finally { CFRelease(data); }
            }
        }

        public static bool TryClear(string credential, out string? error)
        {
            lock (_gate)
            {
                if (!EnsureLoaded(out error)) return false;
                using var cf = new CfScope();
                int status = SecItemDelete(Query(cf, credential));
                error = status is errSecSuccess or errSecItemNotFound ? null : Describe(status);
                return error == null;
            }
        }

        private static string Account(string credential) => $"{credential}@{AppPaths.DataRoot}";

        private static IntPtr Query(CfScope cf, string credential) => cf.Dict(
            new[] { kSecClass, kSecAttrService, kSecAttrAccount },
            new[] { kSecClassGenericPassword, cf.Str(Service), cf.Str(Account(credential)) });

        // Caller holds _gate.
        private static bool EnsureLoaded(out string? error)
        {
            error = _unavailable;
            if (_loaded) return true;
            if (error != null) return false;
            try
            {
                IntPtr sec = NativeLibrary.Load(SecurityLib);
                IntPtr cfl = NativeLibrary.Load(CoreFoundationLib);
                IntPtr Var(IntPtr lib, string name) => Marshal.ReadIntPtr(NativeLibrary.GetExport(lib, name));
                kSecClass                = Var(sec, "kSecClass");
                kSecClassGenericPassword = Var(sec, "kSecClassGenericPassword");
                kSecAttrService          = Var(sec, "kSecAttrService");
                kSecAttrAccount          = Var(sec, "kSecAttrAccount");
                kSecAttrLabel            = Var(sec, "kSecAttrLabel");
                kSecValueData            = Var(sec, "kSecValueData");
                kSecReturnData           = Var(sec, "kSecReturnData");
                kSecMatchLimit           = Var(sec, "kSecMatchLimit");
                kSecMatchLimitOne        = Var(sec, "kSecMatchLimitOne");
                kCFBooleanTrue           = Var(cfl, "kCFBooleanTrue");
                _keyCallBacks   = NativeLibrary.GetExport(cfl, "kCFTypeDictionaryKeyCallBacks");
                _valueCallBacks = NativeLibrary.GetExport(cfl, "kCFTypeDictionaryValueCallBacks");
                _loaded = true;
                return true;
            }
            catch (Exception ex) when (ex is DllNotFoundException or EntryPointNotFoundException)
            {
                error = _unavailable = $"Security.framework unavailable: {ex.Message}";
                return false;
            }
        }

        private static string Describe(int status)
        {
            IntPtr msg = IntPtr.Zero;
            try
            {
                msg = SecCopyErrorMessageString(status, IntPtr.Zero);
                if (msg != IntPtr.Zero)
                {
                    var buf = new byte[512];
                    if (CFStringGetCString(msg, buf, buf.Length, kCFStringEncodingUTF8))
                        return $"{Encoding.UTF8.GetString(buf, 0, Array.IndexOf(buf, (byte)0) is int n and >= 0 ? n : buf.Length)} (OSStatus {status})";
                }
            }
            catch { }
            finally { if (msg != IntPtr.Zero) CFRelease(msg); }
            return $"Keychain error (OSStatus {status})";
        }

        /// <summary>Owns the CF objects created for one call and releases them together.</summary>
        private sealed class CfScope : IDisposable
        {
            private readonly System.Collections.Generic.List<IntPtr> _owned = new();

            private IntPtr Own(IntPtr cf)
            {
                if (cf == IntPtr.Zero) throw new InvalidOperationException("CoreFoundation allocation failed");
                _owned.Add(cf);
                return cf;
            }

            public IntPtr Str(string s)
            {
                byte[] utf8 = Encoding.UTF8.GetBytes(s + "\0");
                return Own(CFStringCreateWithCString(IntPtr.Zero, utf8, kCFStringEncodingUTF8));
            }

            public IntPtr Data(byte[] bytes) => Own(CFDataCreate(IntPtr.Zero, bytes, bytes.Length));

            public IntPtr Dict(IntPtr[] keys, IntPtr[] values) =>
                Own(CFDictionaryCreate(IntPtr.Zero, keys, values, keys.Length, _keyCallBacks, _valueCallBacks));

            public void Dispose()
            {
                // Dictionaries retain their members, so release order does not matter.
                foreach (IntPtr cf in _owned) CFRelease(cf);
                _owned.Clear();
            }
        }
    }
}
