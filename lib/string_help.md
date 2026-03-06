
---

# Custom String & String_View

This is a fast, immutable, Little-Endian optimized string class for our search engine. It avoids heap allocations for short strings (Small String Optimization). **Because it's built for speed, it behaves differently than `std::string`.**

This is similar to Rust's ownership model, this library strictly separates memory-owning strings (string) from zero-copy borrows (string_view). By completely disabling implicit copies, it forces you to explicitly move data or borrow it, guaranteeing zero hidden heap allocations or memory copies.


## The Golden Rules

**1. You cannot copy strings.**
To prevent hidden performance hits, copying is completely disabled. You must move them or pass a `string_view`.

```cpp
string a("hello");
string b = a;            // ❌ COMPILER ERROR
string c = move(a); // ✅ OK

```

**2. `string_view` does not own memory**
A `string_view` does **not** own the memory it points to. If the original `string` or the raw network buffer is destroyed or goes out of scope, any `string_view` pointing to it becomes a dangling pointer. Reading from it will return garbage or cause a segfault.


**3. You cannot use standard array `new`.**
`new string[10]` will fail because there is no default constructor and copying is deleted. You must allocate raw memory and use **placement new**:

```cpp
// Allocate raw memory for 10 strings
void* raw = ::operator new(10 * sizeof(string));
string* arr = static_cast<string*>(raw);

// Construct in place
new (&arr[0]) string("first item"); 

// Note: You must manually call destructors before freeing!
arr[0].~string();
::operator delete(raw);

```

**4. `string_view::data()` is NOT a C-string.**
A `string_view` is just a memory slice. It does **not** have a `\0` (null terminator) at the end.

* ❌ `open(my_view.data(), O_RDONLY)` will read out of bounds and likely segfault.
* ✅ Use the `write_to()` helper function to safely copy the slice into a null-terminated buffer before passing it to C-APIs.

---

##  Workflow (Zero-Copy)

This string is designed to avoid memory allocations whenever possible. When processing data, follow this type of lifecycle when reasonable:

1. **Load:** A network call or disk read dumps the raw request/document into a giant `char` buffer.
2. **Parse in Place:** As you tokenize the data, create `string_view`s that point directly into that original buffer. **Do not create `string` objects here.** This keeps parsing blisteringly fast with zero allocations.
3. **Copy Only When Absolutely Necessary:** Only construct a `string` (via `view.to_string()`) if you need to take ownership of the data—for instance, if you need to save a specific keyword into the index, but the original network buffer is about to be overwritten or destroyed.

---

## ️ Quick Cheatsheet

* **Writing to Buffers:** If you need to send data out (like writing a response to a network socket), use `write_to`. It safely copies the data and adds a null terminator, returning a pointer to the next free byte.
```cpp
char out_buf[256];
char* next_ptr = write_to(out_buf, my_view); 

```


* **Concatenation:** There is no `+` operator. Use the variadic `join` function. It calculates the total size and allocates exactly once:
```cpp
string res = string::join(" ", str1, view2, "text");

```


* **Integers to String:** Just pass a `uint32_t`. It is heavily optimized, holds up to 10 digits, and avoids the heap:
```cpp
string num(404);

```


* **Converting View to String:** Use this when you actually need to copy the data out of a temporary buffer:
```cpp
string s = my_view.to_string();

```

* **Comparisons:** You can safely use `==` to compare any combination of `string`, `string_view`, and standard `"c_strings"`.

---
