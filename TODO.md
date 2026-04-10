Idea:
The final project which we propose will be a xv6-GUI, inspired by the macOS user interface. This will be built on top of the xv6 teaching operating system and operate as a way to interact with files, modify, and create new processes with xv6 via a GUI. Right now xv6 is just a test based terminal with no visual layer to interact with the files, so that is why we want to build this. Some key features of our project will be a window manager capable of rendering multiple overlapping windows, a functional file explorer for navigating the xv6 filesystem, and an embedded terminal emulator for issuing shell commands, all of which will be styled after macOS.

The core systems contributions include:
•	A VGA/framebuffer driver that enables pixel-level drawing in xv6's kernel.
•	A compositing window manager that supports multiple concurrent windows with focus.
•	A file explorer UI backed by real xv6 filesystem syscalls (open, read, readdir).
•	A lightweight terminal window that proxies input/output to xv6's existing shell.
•	Background rendering with selectable wallpaper assets stored on the xv6 filesystem.


Week 1:
- [ ] Purv: VGA framebuffer driver in xv6 kernel; pixel draw primitives (lines, rectangles, fill). 
- [x] Albert: Basic window struct, event loop skeleton.

Week 2:
- [x] Albert: Window manager; create/destroy/focus windows, z-ordering, compositing. 
- [ ] Purv: Keyboard/mouse input routing to focused window.

Week 3:
- [ ] Albert: File explorer (readdir integration), terminal window, 
- [ ] Purv: wallpaper support, macOS-style dock/menu bar. 

Demo polish & final writeup.
