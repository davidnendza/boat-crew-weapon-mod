// IL2CPP C API: exports, class/field/method lookups, string and array helpers (a fragment of weaponmod.cpp, included in order, not standalone)
// ---- IL2CPP C API ------------------------------------------------------------------------------------------
typedef void* (*fn_domain_get)();
typedef void* (*fn_thread_attach)(void*);
typedef void** (*fn_domain_get_assemblies)(void*, size_t*);
typedef void* (*fn_assembly_get_image)(void*);
typedef void* (*fn_class_from_name)(void*, const char*, const char*);
typedef void* (*fn_class_get_method_from_name)(void*, const char*, int);
typedef void* (*fn_class_get_field_from_name)(void*, const char*);
typedef size_t (*fn_field_get_offset)(void*);
typedef const void* (*fn_field_get_type)(void*);
typedef void* (*fn_runtime_invoke)(void*, void*, void**, void**);
typedef void* (*fn_object_get_class)(void*);
typedef const char* (*fn_class_get_name)(void*);
typedef void* (*fn_class_get_methods)(void*, void**);
typedef const char* (*fn_method_get_name)(void*);
typedef uint32_t (*fn_method_get_param_count)(void*);
typedef const void* (*fn_method_get_param)(void*, uint32_t);
typedef const char* (*fn_type_get_name)(const void*);
typedef void* (*fn_string_new)(const char*);
typedef void* (*fn_class_get_parent)(void*);
typedef void* (*fn_object_new)(void*);
typedef void* (*fn_class_get_nested_types)(void*, void**);
typedef void* (*fn_class_from_type)(const void*);
typedef bool (*fn_class_is_enum)(void*);
typedef const void* (*fn_class_enum_basetype)(void*);
typedef void (*fn_field_static_get_value)(void*, void*);
typedef const void* (*fn_class_get_type)(void*);
typedef void* (*fn_type_get_object)(const void*);
typedef void* (*fn_gchandle_new)(void*, bool);                     // Il2CppGCHandle is pointer-sized in this IL2CPP
typedef void (*fn_gchandle_free)(void*);
typedef void (*fn_gc_wbarrier_set_field)(void*, void**, void*);
typedef bool (*fn_gc_is_incremental)();
typedef void* (*fn_get_corlib)();

static fn_domain_get il2cpp_domain_get;
static fn_thread_attach il2cpp_thread_attach;
static fn_domain_get_assemblies il2cpp_domain_get_assemblies;
static fn_assembly_get_image il2cpp_assembly_get_image;
static fn_class_from_name il2cpp_class_from_name;
static fn_class_get_method_from_name il2cpp_class_get_method_from_name;
static fn_class_get_field_from_name il2cpp_class_get_field_from_name;
static fn_field_get_offset il2cpp_field_get_offset;
static fn_field_get_type il2cpp_field_get_type;
static fn_runtime_invoke il2cpp_runtime_invoke;
static fn_object_get_class il2cpp_object_get_class;
static fn_class_get_name il2cpp_class_get_name;
static fn_class_get_methods il2cpp_class_get_methods;
static fn_method_get_name il2cpp_method_get_name;
static fn_method_get_param_count il2cpp_method_get_param_count;
static fn_method_get_param il2cpp_method_get_param;
static fn_type_get_name il2cpp_type_get_name;
static fn_string_new il2cpp_string_new;
static fn_class_get_parent il2cpp_class_get_parent;
static fn_object_new il2cpp_object_new;
static fn_class_get_nested_types il2cpp_class_get_nested_types;
static fn_class_from_type il2cpp_class_from_type;
static fn_class_is_enum il2cpp_class_is_enum;
static fn_class_enum_basetype il2cpp_class_enum_basetype;
static fn_field_static_get_value il2cpp_field_static_get_value;
static fn_class_get_type il2cpp_class_get_type;
static fn_type_get_object il2cpp_type_get_object;
static fn_gchandle_new il2cpp_gchandle_new;                         // optional: looked up without failing Resolve
static fn_gchandle_free il2cpp_gchandle_free;
static fn_gc_wbarrier_set_field il2cpp_gc_wbarrier_set_field;      // optional: Unity's incremental GC needs it for reference stores
static fn_gc_is_incremental il2cpp_gc_is_incremental;
static fn_get_corlib il2cpp_get_corlib;                             // optional: a plain read, null until il2cpp_init has run

static HMODULE g_ga = nullptr;
static void* g_domain = nullptr;

static bool Resolve() {
  bool ok = true;
#define R(name) name = (decltype(name))GetProcAddress(g_ga, #name); if (!name) { Log("missing export: " #name); ok = false; }
  R(il2cpp_domain_get) R(il2cpp_thread_attach) R(il2cpp_domain_get_assemblies) R(il2cpp_assembly_get_image)
  R(il2cpp_class_from_name) R(il2cpp_class_get_method_from_name) R(il2cpp_class_get_field_from_name)
  R(il2cpp_field_get_offset) R(il2cpp_field_get_type) R(il2cpp_runtime_invoke) R(il2cpp_object_get_class)
  R(il2cpp_class_get_name) R(il2cpp_class_get_methods) R(il2cpp_method_get_name) R(il2cpp_method_get_param_count)
  R(il2cpp_method_get_param) R(il2cpp_type_get_name) R(il2cpp_string_new) R(il2cpp_class_get_parent)
  R(il2cpp_object_new) R(il2cpp_class_get_nested_types) R(il2cpp_class_from_type) R(il2cpp_class_is_enum)
  R(il2cpp_class_enum_basetype) R(il2cpp_field_static_get_value) R(il2cpp_class_get_type) R(il2cpp_type_get_object)
#undef R
  il2cpp_gchandle_new = (fn_gchandle_new)GetProcAddress(g_ga, "il2cpp_gchandle_new");
  il2cpp_gchandle_free = (fn_gchandle_free)GetProcAddress(g_ga, "il2cpp_gchandle_free");
  if (!il2cpp_gchandle_new || !il2cpp_gchandle_free) Log("il2cpp_gchandle_new/free not exported; scanned UI lists are not held");
  il2cpp_gc_wbarrier_set_field = (fn_gc_wbarrier_set_field)GetProcAddress(g_ga, "il2cpp_gc_wbarrier_set_field");
  il2cpp_gc_is_incremental = (fn_gc_is_incremental)GetProcAddress(g_ga, "il2cpp_gc_is_incremental");
  il2cpp_get_corlib = (fn_get_corlib)GetProcAddress(g_ga, "il2cpp_get_corlib");
  if (!il2cpp_get_corlib) Log("il2cpp_get_corlib not exported; startup waits on the assembly list only");
  return ok;
}

// il2cpp_domain_get CREATES the domain with a GC allocation when there is none yet. Called before the game's il2cpp_init,
// that starts the garbage collector on this thread and the game then dies with "Fatal error in GC: Exclusion ranges
// overlap" / "Threads explicit registering is not previously enabled" (the friend's join-from-Steam launch, 2026-10-31).
// So it is only called once two plain reads show the runtime is up: the assembly list (il2cpp_domain_get_assemblies only
// reads a global vector and ignores its domain argument) and il2cpp_get_corlib. Runtime::Init sets corlib after starting
// the GC and creates the domain a few class lookups later, so corlib must have been there for a second already.
static bool DomainReady(void** domain) {
  static DWORD corlibSince = 0;
  __try {
    size_t n = 0;
    void** a = il2cpp_domain_get_assemblies(nullptr, &n);
    if (!a || n < 10) return false;
    if (il2cpp_get_corlib && !il2cpp_get_corlib()) { corlibSince = 0; return false; }
    if (!corlibSince) corlibSince = GetTickCount();
    if (GetTickCount() - corlibSince < 1000) return false;
    void* d = il2cpp_domain_get();
    if (!d) return false;
    *domain = d; return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static void* FindClass(const char* ns, const char* name) {
  size_t n = 0;
  void** asms = il2cpp_domain_get_assemblies(g_domain, &n);
  for (size_t i = 0; i < n; i++) {
    void* img = il2cpp_assembly_get_image(asms[i]);
    if (!img) continue;
    void* k = il2cpp_class_from_name(img, ns, name);
    if (k) return k;
  }
  return nullptr;
}

// A field's offset in its object, or -1 when the class or the field is missing.
static int FieldOffset(void* klass, const char* field) { void* f = klass ? il2cpp_class_get_field_from_name(klass, field) : nullptr; return f ? (int)il2cpp_field_get_offset(f) : -1; }
static int FieldOffset(const char* ns, const char* cls, const char* field) { return FieldOffset(FindClass(ns, cls), field); }

// Il2CppArray: the length (il2cpp_array_size_t) sits at 0x18 and the elements start at 0x20; reference elements are 8 bytes.
static int ArrLen(void* a) { return a ? *(int*)((char*)a + 0x18) : 0; }
static void* ArrData(void* a) { return (char*)a + 0x20; }
static void* ArrAt(void* a, size_t i) { return *(void**)((char*)a + 0x20 + 8 * i); }

// The text with every "\n" written as nl (drops.ini keeps the line ends the player's file already has).
static std::string WithNewlines(const std::string& s, const std::string& nl) {
  if (nl != "\r\n") return s;
  std::string c; for (char ch : s) { if (ch == '\n') c += '\r'; c += ch; } return c;
}

// A type's name. il2cpp_type_get_name returns a copy the caller must free with il2cpp_free (it leaked a little on every
// edit of every boat build until 2026-10-28).
static std::string TypeName(const void* type) {
  static void (*freeFn)(void*) = (void (*)(void*))GetProcAddress(g_ga, "il2cpp_free");
  const char* n = type ? il2cpp_type_get_name(type) : nullptr;
  std::string s = n ? n : "";
  if (n && freeFn) freeFn((void*)n);
  return s;
}

// Method by name and parameter count, optionally checking the first parameter types (il2cpp type names).
static void* FindMethod(void* klass, const char* name, int nparams, const char* p0 = nullptr, const char* p1 = nullptr) {
  for (void* k = klass; k; k = il2cpp_class_get_parent(k)) {
    void* iter = nullptr; void* m;
    while ((m = il2cpp_class_get_methods(k, &iter))) {
      if (strcmp(il2cpp_method_get_name(m), name)) continue;
      if ((int)il2cpp_method_get_param_count(m) != nparams) continue;
      if (p0 && TypeName(il2cpp_method_get_param(m, 0)) != p0) continue;
      if (p1 && TypeName(il2cpp_method_get_param(m, 1)) != p1) continue;
      return m;
    }
  }
  return nullptr;
}

// Store a managed reference into a field (or array slot) of a managed object. Unity 6's incremental GC can otherwise miss
// a reference written in the middle of its mark phase and free the object (code review 2026-10-24, finding 1).
static void SetRef(void* obj, void* addr, void* value) {
  if (il2cpp_gc_wbarrier_set_field) il2cpp_gc_wbarrier_set_field(obj, (void**)addr, value);
  else *(void**)addr = value;
}

// GC handles for managed objects the mod keeps across frames (cached sprites, the white texture, hidden UI rows);
// a handle keeps the object from being collected. Pin returns the handle (null if the export is missing).
static void* Pin(void* obj) { return obj && il2cpp_gchandle_new ? il2cpp_gchandle_new(obj, false) : nullptr; }
static void Unpin(void* handle) { if (handle && il2cpp_gchandle_free) il2cpp_gchandle_free(handle); }
static void* PlayerCraft();                                          // Player.playerCraft, the player's boat (method cached)

static void* Invoke(void* method, void* obj, void** args = nullptr) {
  void* exc = nullptr;
  void* r = il2cpp_runtime_invoke(method, obj, args, &exc);
  return exc ? nullptr : r;
}

// Il2Cpp string -> UTF-8. Four rotating buffers so several calls can share one Log line.
static const char* Str(void* s) {
  static char bufs[4][256]; static int n = 0;
  char* buf = bufs[n++ & 3];
  if (!s) { strcpy_s(buf, 256, "(null)"); return buf; }
  int len = *(int*)((char*)s + 0x10);
  const wchar_t* w = (const wchar_t*)((char*)s + 0x14);
  int c = WideCharToMultiByte(CP_UTF8, 0, w, len, buf, 255, nullptr, nullptr);
  buf[c < 0 ? 0 : c] = 0; return buf;
}
// List<T> layout: _items at 0x10 (array: length 0x18, elements from 0x20), _size at 0x18
static int ListCount(void* list) { return list ? *(int*)((char*)list + 0x18) : 0; }
static void* ListAt(void* list, int i) { void* arr = *(void**)((char*)list + 0x10); return ArrAt(arr, i); }
static bool ListAdd(void* list, void* item) {
  void* add = il2cpp_class_get_method_from_name(il2cpp_object_get_class(list), "Add", 1);
  if (!add) return false;
  void* args[1] = { item };
  void* exc = nullptr; il2cpp_runtime_invoke(add, list, args, &exc);
  return exc == nullptr;
}
static bool ListSetAt(void* list, int i, void* v) {
  void* m = il2cpp_class_get_method_from_name(il2cpp_object_get_class(list), "set_Item", 2);
  void* a[2] = { &i, v }; void* exc = nullptr;
  if (m) il2cpp_runtime_invoke(m, list, a, &exc);
  return m && !exc;
}
static bool ListRemoveAt(void* list, int i) {
  void* m = il2cpp_class_get_method_from_name(il2cpp_object_get_class(list), "RemoveAt", 1);
  void* a[1] = { &i }; void* exc = nullptr;
  if (m) il2cpp_runtime_invoke(m, list, a, &exc);
  return m && !exc;
}

