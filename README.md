# clippy: A shared clipboard manager with history for your desktop and servers
Including history.
Stored securely on one of your servers.

Can be made with `ssh -R /tmp/clipboardremote:/tmp/clipboardlocal host` (tests can be done with `nc -lU /tmp/clipboardserver` and `nc -U /tmp/clipboardclient`).

A problem is that if  the /tmp/clipboardclient already exists (after an old session), the proxy of the port won't work. `-o 'StreamLocalBindUnlink=yes'` is supposed to help, however, at least for `-R` mode, it does not. See https://bugzilla.mindrot.org/show_bug.cgi?id=2601 . It works if `StreamLocalBindUnlink yes` is specified in the server config.

# Usage

Clippy should be installed on both the client and the server. Use `clippy --ssh` instead of `ssh` to connect to remote computers. Clippy set additional options automatically for ssh.

# Local (desktop)

Generates a UNIX domain socket on `/tmp/clipboard.username`, which is forwared. The daemon should have a config file to configure the local actions.

# Remote

Command line utilities to piggy back files and clipboard commands to the server.

Setup depends on the software you are using:

## sshd (on some platforms set automatically)

Your `sshd_config` on the server should have:
```
StreamLocalBindUnlink yes
AcceptEnv LC_CLIPPY
```

## tmux
Use `tmux-yank` with:
```
# move x clipboard into tmux paste buffer
bind ] run "tmux set-buffer \"clippy -g)\"; tmux paste-buffer"
set -g @override_copy_command 'clippy -s'
set-option -g -a update-environment "LC_CLIPPY"
```

The basic version:
```
bind ] run "tmux set-buffer \"$(clippy -g)\"; tmux paste-buffer"
# move tmux copy buffer into x clipboard
bind -t vi-copy y run "tmux save-buffer - | clippy -s"
bind -t emacs-copy y run "tmux save-buffer - | clippy -s"
set-option -g -a update-environment "LC_CLIPPY"
```

## neovim
```
clipboard+=unnamed,unnamedplus " shared 
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

For custom `neovim` clipboard, see `:help g:clipboard`.

# Supported commands

- [x] read clipboard
- [x] set clipboard
- [ ] view file on desktop
- [ ] open URL in browser on desktop
- [ ] show message on desktop (with DBus)
- [ ] copy file to/from desktop
- [ ] support rendering part of a i3statusbar on desktop: cpu usage, memory usage, custom things, workqueue like nq status (with remote queue)
- [ ] remote dmenu session

ToDo:
- [ ] support connecting from different machines to the same server (probably should do something with randomized /tmp/clipboardremote.some-unique-number, and setting a environment variable which clipboard connection to use. See TOKENS in `man ssh_config`)
- [ ] ability to keep clipboard when changing users (e.g. root user). Relevant: passing environment variables: https://superuser.com/a/480029 . Can be used to indicate a specific socket. (Still have to solve the UNIX socket permissions)
- [ ] Support Wayland clipboard (without X, e.g. in foot terminal)
