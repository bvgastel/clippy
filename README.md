# clippy: making clipboard work over SSH

Clippy is a tool to interact with your desktop environment from a remote SSH sessions: share clipboard contents, and show notifications on your desktop. This way, your remote neovim session can paste directly from your clipboard, and yanking in a remote session will end up in your local clipboard manager (e.g. clipmenud). Clippy uses the SSH connection (specifically UNIX domain socket forwarding), so the clipboard contents is encrypted during transmission.

Integrates with:
- Linux/FreeBSD (X11/Wayland);
- Windows if run under WSL;
- macOS.

# Installation
For FreeBSD, Debian, Ubuntu, and Raspbian, add the [bitpowder repository](https://bitpowder.com:2443/bitpowder/repo). See the instructions on that repository page.

For macOS:
```
brew tap bvgastel/clippy https://github.com/bvgastel/clippy
brew install --HEAD bvgastel/clippy/clippy
```
Update with `brew reinstall --HEAD bvgastel/clippy/clippy`.


To build the project from the source, you need cmake and a C++ compiler.

# Usage

Clippy should be installed on both the client and the server. Use `clippy --ssh` instead of `ssh` to connect to remote computers. Clippy set additional options automatically for ssh.
If no clippy daemon is running, it automatically starts a clippy daemon that listens on a UNIX domain socket named `/tmp/clipboard.username.something`, which is forwarded through SSH.

To **retrieve the clipboard** contents, use `clippy -g`.

To **set the clipboard**, use `echo contents | clippy -s`.

To show a **notification**, use `clippy -n [summary] [body]`.

# Set up for specific programs

Command line utilities to piggyback files and clipboard commands to the server. Some software needs custom config options. Setup depends on the software you are using:

## sshd (on some platforms set automatically)

Your `sshd_config` on the server should have:
```
StreamLocalBindUnlink yes
AcceptEnv LC_CLIPPY
```

On modern Debian, Ubuntu, and Raspbian, these settings are automatically included when you install `clippy`.
On FreeBSD add `Include /usr/local/etc/ssh/sshd_config.d/*`.
On Linux add `Include /etc/ssh/sshd_config.d/*`.

## tmux
Use with `tmux-yank` (in `~/.tmux.conf`):
```
# move x clipboard into tmux paste buffer
bind ] run "tmux set-buffer \"$(clippy -g)\"; tmux paste-buffer"
set -g @override_copy_command 'clippy -s'
set-option -g -a update-environment "LC_CLIPPY"
```

The basic version, without any plugins (in `~/.tmux.conf`):
```
bind ] run "tmux set-buffer \"$(clippy -g)\"; tmux paste-buffer"
# move tmux copy buffer into x clipboard
bind -t vi-copy y run "tmux save-buffer - | clippy -s"
bind -t emacs-copy y run "tmux save-buffer - | clippy -s"
set-option -g -a update-environment "LC_CLIPPY"
```

## neovim
In `.config/nvim/init.vim`:
```
set clipboard+=unnamed,unnamedplus
let g:clipboard = {
      \   'name': 'ClippyRemoteClipboard',
      \   'copy': {
      \      '+': 'clippy -s',
      \      '*': 'clippy -s',
      \    },
      \   'paste': {
      \      '+': 'clippy -g'
      \      '*': 'clippy -g',
      \   },
      \   'cache_enabled': 0,
      \ }
```

For more custom `neovim` clipboard settings, see `:help g:clipboard`.

# Supported commands

- [x] read clipboard
- [x] set clipboard
- [x] show message on desktop (with `notify-send`, on WSL use `powershell.exe`, on macOS use `osascript`)
- [ ] view file on desktop
- [ ] open URL in browser on desktop
- [ ] copy file to/from desktop
- [ ] support rendering part of a i3statusbar on desktop: cpu usage, memory usage, custom things, work queue like nq status (with remote queue)
- [ ] remote dmenu session
