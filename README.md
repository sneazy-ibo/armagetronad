# Armagetron Advanced

## User Documentation

All the documentation for Armagetron is now in the src/doc/ directory and in HTML.
Starting point is index.html. In source distributions, all these files are in
a homemade template format using the m4 macro language. 

In installed version, the documentation directory is usually located in /usr/local/games/armagetronad.

Better documentation is on our [Wiki](https://wiki.armagetronad.org/index.php?title=PlayingGettingStarted).

## Community

- Most of the activity nowadays is on Discord, use [this invite](https://discord.gg/dcpaauj).
- Much slower: [The Forums](https://forums3.armagetronad.net/).
- In IRC, some of the old guard are in the channel `#armagetron` on trhe OFTC network. Have patience there, responses can take a while.

## Developer Documentation

### Unix Systems (Linux, *BSD, macOS)

This project uses autoconf/automake. Run `.\bootstrap.sh` to generate the scripts, then `configure` as appropriate. The most important argument is `--enable-dedicated`, which switches compilation from the default client to the dedicated server.

If you do do not mind opinionated builds, run `./batch/test_builds.sh default`, this makes server and client builds in the `build` subdirectory.

Read more on dependencies you may need [Wiki](https://wiki.armagetronad.org/index.php?title=Linux_Development).

### Windows

Too much for this readme, check the [Wiki](https://wiki.armagetronad.org/index.php?title=Windows_Development).

## Contributing

This is an open source game, contributions are generally welcome! Read [CONTRIBUTING.md](CONTRIBUTING.md) before jumping in head first.