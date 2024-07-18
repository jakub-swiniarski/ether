# Ether
An overly minimal Vim-like editor <br/>
Ether uses parts of code from [Kilo](https://github.com/antirez/kilo). Kilo's license is included in this repository.

## How to install?
Run this as root:
```shell
make install
```
Start editing:
```shell
touch file
ether file
```

## How to uninstall?
Run this as root:
```shell
make uninstall
```

## Default keybindings
### In normal mode
Colon (:) - enter command mode
I - enter insert mode
H - move cursor to the left
J - move cursor down
K - move cursor up
L - move cursor to the right

### In command mode
Q - quit
W - save changes
X - delete character
D - delete row

### In any mode
Escape - enter normal mode

## Customization
Edit config.h, recompile & reinstall the program.
