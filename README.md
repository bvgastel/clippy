# clippy: A shared clipboard manager with history for your desktop and servers
Including history.
Stored securely on one of your servers.

Can be made with `ssh -R /tmp/clipboardremote:/tmp/clipboardlocal host` (tests can be done with `nc -lU /tmp/clipboardserver` and `nc -U /tmp/clipboardclient`).

A problem is that if  the /tmp/clipboardclient already exists (after an old session), the proxy of the port won't work. `-o 'StreamLocalBindUnlink=yes'` is supposed to help, however, at least for `-R` mode, it does not. See https://bugzilla.mindrot.org/show_bug.cgi?id=2601 . It works if `StreamLocalBindUnlink yes` is specified in the server config.


For custom `neovim` clipboard, see `:help g:clipboard`.

# Local (desktop)

Generates a UNIX domain socket on /tmp/clipboard. Once connected, the UNIX domain socket is moved to a temporary location `/tmp/clipboard.32235`, and a new fresh UNIX domain socket is created at /tmp/clipboard.

The daemon should have a config file to configure the local actions. This can be a Lua file.

Additionally, your `ssh_config` should have:
```
RemoteForward /tmp/clipboardremote.%r /tmp/clipboardlocal.%u
StreamLocalBindUnlink yes
```

# Remote

Command line utilities to piggy back files and clipboard commands to the server.

Setup depends on the software you are using:

## tmux
Use `tmux-yank` with:
```
# move x clipboard into tmux paste buffer
bind ] run "tmux set-buffer \"clippy-remote -g)\"; tmux paste-buffer"
set -g @override_copy_command 'clippy-remote -s'
```

The basic version:
```
bind ] run "tmux set-buffer \"$(clippy-remote -g)\"; tmux paste-buffer"
# move tmux copy buffer into x clipboard
bind -t vi-copy y run "tmux save-buffer - | clippy-remote -s"
bind -t emacs-copy y run "tmux save-buffer - | clippy-remote -s"
```

## neovim
```
clipboard+=unnamed,unnamedplus " shared 
let g:clipboard = {
      \   'name': 'ClippyRemoteClipboard',
      \   'copy': {
      \      '+': 'clippy-remote -s',
      \      '*': 'clippy-remote -s',
      \    },
      \   'paste': {
      \      '+': 'clippy-remote -g'
      \      '*': 'clippy-remote -g',
      \   },
      \   'cache_enabled': 0,
      \ }
```
# Supported commands

- [x] read clipboard
- [x] set clipboard
- [ ] view file on desktop
- [ ] open URL in browser on desktop
- [ ] show message on desktop (with DBus)
- [ ] copy file to/from desktop
- [ ] support rendering part of a i3statusbar on desktop: cpu usage, memory usage, custom things, workqueue like nq status (with remote queue)

ToDo:
- [ ] support connecting from different machines to the same server (probably should do something with randomized /tmp/clipboardremote.some-unique-number, and setting a environment variable which clipboard connection to use. See TOKENS in `man ssh_config`)
- [ ] ability to keep clipboard when changing users (e.g. root user). Relevant: passing environment variables: https://superuser.com/a/480029 . Can be used to indicate a specific socket. (Still have to solve the UNIX socket permissions)
