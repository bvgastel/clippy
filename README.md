# clippy: making clipboard work over SSH

Clippy is a tool to interact with your desktop environment from a remote SSH sessions: share clipboard contents, open URLs and show notifications on your desktop. This way, your remote neovim session can paste directly from your clipboard, and yanking in a remote session will end up in your local clipboard manager (e.g. clipmenud). Clippy uses the SSH connection (specifically UNIX domain socket forwarding), so the clipboard contents is encrypted during transmission.

This is an alternative to [lemonade](https://github.com/lemonade-command/lemonade). However, the security on shared servers is improved as only the current user can share the link to the desktop.

Integrates with:
- Linux/FreeBSD (for X11 needs package `xsel`, for Wayland needs package `wl-clipboard`);
- Termux on Android;
- Windows if run under WSL;
- macOS.

## Installation
For FreeBSD, Debian, Ubuntu, and Raspbian, add the [bitpowder repository](https://bitpowder.com:2443/bitpowder/repo). See the instructions on that repository page.

For macOS:
```
brew tap bvgastel/clippy https://github.com/bvgastel/clippy
brew install --HEAD bvgastel/clippy/clippy
```
Update with `brew reinstall bvgastel/clippy/clippy`.


To build the project from the source, you need cmake and a C++ compiler:
```
cmake .
cmake --build .
cmake --install .
```

On Termux (Android) use:
```
cmake -DCMAKE_INSTALL_PREFIX=$PREFIX .
cmake --build .
cmake --install .
```

## Usage

Clippy should be installed on both the client and the server. Use `clippy ssh` instead of `ssh` to connect to remote computers. Clippy set some additional options automatically for ssh.
If no clippy daemon is running, it automatically starts a clippy daemon that listens on a UNIX domain socket named `/tmp/clipboard.username.something`, which is forwarded through SSH.

Commands:
- `clippy ssh` to **set up a clippy ssh connection** to a different host;
- `clippy get` to **retrieve the clipboard** contents;
- `echo contents | clippy set` to **set the clipboard**;
- `clippy notification [summary] [body]` to show a **notification**.
- `clippy openurl [url]` to **open an URL in your default browser**.
- `clippy command [command] [args]` to **execute a command on the desktop**, such as a dmenu session. It forwards stdin, stdout, stderr, and copies over the exit code. Terminal sizes etc are not copied over.

If you execute these commands locally, they also work. Making them suitable for integration in generic config files used both for a server and a client.

## Set up for specific programs

Some software needs custom config options. Setup depends on the software you are using:

### sshd (on some platforms set automatically)

On modern Debian (>= bullseye), Ubuntu (>= focal), and Raspbian (>= bullseye), these settings are automatically included when you install `clippy`. 

Your `sshd_config` on the server should have:
```
StreamLocalBindUnlink yes
AcceptEnv LC_CLIPPY
```

On Linux add `Include /etc/ssh/sshd_config.d/*` to your `/etc/ssh/sshd_config`.
On FreeBSD add `Include /usr/local/etc/ssh/sshd_config.d/*` to your `/etc/ssh/sshd_config`.

### tmux
Use with `tmux-yank` (in `~/.tmux.conf`):
```
# move x clipboard into tmux paste buffer
bind ] run "tmux set-buffer \"$(clippy get)\"; tmux paste-buffer"
set -g @override_copy_command 'clippy set'
set-option -g -a update-environment "LC_CLIPPY"
```

The basic version, without any plugins (in `~/.tmux.conf`):
```
bind ] run "tmux set-buffer \"$(clippy get)\"; tmux paste-buffer"
# move tmux copy buffer into x clipboard
bind -T copy-mode-vi y run "tmux save-buffer - | clippy set"
bind -T copy-mode-emacs y run "tmux save-buffer - | clippy set"
set-option -g -a update-environment "LC_CLIPPY"
```

### neovim
In `.config/nvim/init.vim`:
```
set clipboard+=unnamed,unnamedplus
let g:clipboard = {
      \   'name': 'ClippyRemoteClipboard',
      \   'copy': {
      \      '+': 'clippy set',
      \      '*': 'clippy set',
      \    },
      \   'paste': {
      \      '+': 'clippy get'
      \      '*': 'clippy get',
      \   },
      \   'cache_enabled': 0,
      \ }
```

For more custom `neovim` clipboard settings, see `:help g:clipboard`.

## Supported commands

- [x] read clipboard
- [x] set clipboard
- [x] show message on desktop (with `notify-send`, on WSL use `powershell.exe`, on macOS use `osascript`)
- [x] open URL in browser on desktop
- [x] remote command, such as a remote dmenu session
- [ ] view file on desktop
- [ ] copy file to/from desktop
- [ ] support rendering part of a i3statusbar on desktop: CPU usage, memory usage, custom things, work queue like nq status (with remote queue)

## To Do
- [ ] improve error handling: errors in the clippy daemon (such as xsel/wl-paste not found) should be propagated to clippy clients (so they can exit with a non zero exit code)
- [ ] 

## Alternatives

- http://sshmenu.sourceforge.net/articles/bcvi/
