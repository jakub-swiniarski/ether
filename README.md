# Ether
An overly minimal Vim-like editor <br/>
Ether uses parts of code from [Kilo](https://github.com/antirez/kilo). Kilo's license is included in this repository.

## How to install?
Run this with root privileges:
```shell
make install
```
Start editing:
```shell
touch file
ether file
```

## How to uninstall?
Run this with root privileges:
```shell
make uninstall
```

## Default keybindings
### In normal mode
Colon (:) - enter command mode <br/>
I - enter insert mode <br/>
H - move cursor to the left <br/>
J - move cursor down <br/>
K - move cursor up <br/>
L - move cursor to the right <br/>

### In command mode
Q - quit <br/>
W - save changes <br/>
X - delete character <br/>
D - delete row <br/>

### In any mode
Escape - enter normal mode <br/>

## Customization
Edit config.h, recompile & reinstall the program.
