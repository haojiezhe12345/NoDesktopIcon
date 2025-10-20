# NoDesktopIcon

**A Windows Shell Extenstion to prevent Desktop icons from loading.**

---

> **Do you have hundreds of files on Desktop, causing lags when logging in, refreshing or adding new files?**  
> **And do you barely use the Desktop to browse files, only browse them in file explorer?**
>
> **If yes, you'll probably need this extension.**

## Introduction

Windows Explorer does have a "Show desktop icons" option to let you hide desktop icons, however, they're just visually hidden and still being refreshed internally. It won't do any help at removing the lag caused by too many files.

**This shell extenstion completely prevents the internal loading of desktop icons by patching `CDefView::_ReloadView` in `shell32.dll`.**

### Patching

In `CDefView::_ReloadView`, we patch the jump condition and the return value:

```asm
; Before patch:
mov     edi, edx            ; 8B FA
mov     rbx, rcx            ; 48 8B D9
mov     esi, 1              ; BE 01 00 00 00    (The return value, initially 1)
jnz     short loc_1800CBEF6 ; 75 ??             (Conditional jump to the return block, '??' means a relative offset)
; (If not jump, it will execute the reloading logic, and set return value to 0 on success)

; After patch:
mov     edi, edx            ; 8B FA
mov     rbx, rcx            ; 48 8B D9
mov     esi, 0              ; BE 00 00 00 00    (Set return value to 0)
jmp     short loc_1800CBEF6 ; EB ??             (Force jump to the return block)
; (The reloading logic never reaches)
```

So out goal is to simply:  
- search for bytes: `8B FA 48 8B D9 BE 01 00 00 00 75`  
- and replace with: `8B FA 48 8B D9 BE 00 00 00 00 EB`

The first 5 bytes is for pricise locating, and the jump offset byte is ignored because it varies across different versions of `shell32.dll`.

After the patch, `CDefView::_ReloadView` will do nothing when called.  
Files in Desktop will no longer be loaded and show up, removing all lags caused by the loading of them.

## The Shell Extension

Directly patching `shell32.dll` inside `system32` is not a good practice, as changes will lost after a Windows Update.  
With a Shell Extension, explorer will auto load our extension DLL, thus we can patch the code inside `explorer.exe` on the fly, before the desktop is loaded.

### The auto load mechanism

To ensure our DLL is loaded at early stage of explorer initiation, we leverage the following registry key:

```
HKEY_CLASSES_ROOT
   Drive
      ShellEx
         FolderExtensions
            {F5CD59F3-8E88-4324-A23D-01EA050EBF72}
               DriveMask = 0x000000ff
```

This registry hack is discovered in [StartAllBack](https://www.startallback.com/), which enables it to load its custom taskbar on explorer initiation.  
*I don't know why this hack work yet, if you know, open a Pull Request, I'll be appreciate.*

Also our DLL should be registered in CLSID:

```
HKEY_CLASSES_ROOT
   CLSID
      {F5CD59F3-8E88-4324-A23D-01EA050EBF72}
         InprocServer32
            (Default) = C:\path\to\ExplorerNoDesktopIcons.dll
            ThreadingModel = Apartment
```

### Inside the DLL

CLSID registered in `HKEY_CLASSES_ROOT\Drive\shellex\FolderExtensions` will be loaded into a wide range of programs, such as `taskmgr.exe`, `svchost.exe`, and even **Chrome**.  
To ensure other program's integrity, we should only inject to `explorer.exe`.

This is done by comparing process image file name, if it does not match, return `FALSE` to immediately unload from that process ([dllmain.cpp:159](./dllmain.cpp#L159)).

#### `DllGetClassObject`

With a well-configred registry, this function will be called at a very early stage of explorer initiation, before Desktop is initiated.  
Also, it is blocking. The initiation process is paused when it is executing, giving us a perfect timing to patch the memory.

For a regular Shell Extension, this function should create an instance of a class object, and return `S_OK`.  
However, since we are writing a feature-less Shell Extension (we're just patching explorer), we can always return `CLASS_E_CLASSNOTAVAILABLE` ([dllmain.cpp:183](./dllmain.cpp#L183)), so that we don't have to care about that class.

### Special thanks

- [StartAllBack](https://www.startallback.com/)
- [Google Gemini](https://gemini.google.com/share/1025d11b2204)
