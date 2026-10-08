# Contributing

Bug reports, patches and ideas are welcome. Please open an
[issue](https://github.com/michaelsternberg/wmcoremap/issues) or a pull
request on GitHub.

## Reporting a bug

Please include:

- the wmcoremap version (`git describe` or the `.deb` version)
- your distribution and window manager
- the number of CPUs (`nproc`) and the CPU model (`lscpu | grep 'Model name'`)
- for temperature problems: `grep . /sys/class/hwmon/hwmon*/name`
- for GPU problems: `ls /sys/class/drm/card*/device/` and the GPU model
- the command line you ran, and a screenshot if the display looks wrong

## Building and testing

```sh
make
./wmcoremap -fake 96        # try a layout without a 96-core machine
```

Before sending a patch, make sure it builds with no warnings:

```sh
make clean && CFLAGS="-O2 -Werror" make
```

The same check runs on every push and pull request in GitHub Actions.

## Code style

- C99, tabs for indentation (see `.editorconfig`).
- Keep to the existing style of the file you are changing.
- No new runtime dependencies beyond libX11 and libXext.
- If you add or change a command-line option, update `README.md`, the man
  page `wmcoremap.1` and the usage text in `src/wmcoremap.c`.
- Add a line to `CHANGELOG.md` under "Unreleased".

## Making a release

1. Move the "Unreleased" entries in `CHANGELOG.md` under the new version.
2. Add an entry to `debian/changelog` (`dch -v X.Y`).
3. Commit, then tag and push: `git tag -a vX.Y -m "wmcoremap X.Y" && git push origin vX.Y`.

The `release` workflow builds the Debian package and publishes it as a
GitHub release.

## License

By contributing you agree that your changes are released under the GNU
General Public License version 2 or later, the same as the rest of
wmcoremap.
