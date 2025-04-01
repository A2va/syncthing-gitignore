
synctignore is tool to help convert gitignore files to syncthing ignore files.

This project is built with cosmopolitan, a build-once-run-anywhere compiler, in mind, but since file watcher is not really supported, cosmocc binaries are not yet available.

> [!TIP]
> If you are using [syncthing-tray](https://github.com/Martchus/syncthingtray), you can set synctignore as an extra launcher next to the tray. This option is located in Syncthing Tray Settings -> Startup -> Extra Launcher.

Acknowledgements:
* Idea and motivation based on this [blog post](https://jupblb.prose.sh/stignore)
* gitignore_parser library from [here](https://github.com/mherrmann/gitignore_parser)

## Building the project

synctignore is based on the [XMake](https://xmake.io) build system.
```
git clone https://github.com/A2va/syncthing-gitignore
cd syncthing-gitignore
xmake
```