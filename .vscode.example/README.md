# Visual Studio Code Configuration Starting Point

This is a sample configuration you can use to build and debug the game straight
from the source folder.

## Setup with customizations, but without updates

Run in the main source folder:

```bash
cp .vscode.example .vscode
```

This copies the configuration to where Visual Studio Code sees it, 
and you can customize it without risk of accidentally commiting your changes.

## Setup with updates, but no easy customizations

Link the folder instead:

```bash
ln -s .vscode.example .vscode
```

git updates will then also update your configuration. Any customizations you do
will go into git if you are not careful, though, but maybe that is what you want.

This is the configuration Z-Man currently is using for development on macOS and Linux.
