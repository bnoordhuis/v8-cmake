# Debug in VSCode

Follow below steps to debug in VSCode:

1. Clone this repo into your local filesystem
2. Install [Docker Desktop](https://www.docker.com/products/docker-desktop) in your environment
3. Increase the running resource limitation such as CPUs or Memory of the container in [Docker Desktop preferences](https://docs.docker.com/docker-for-mac/#preferences)
2. Install [Remote Development](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.vscode-remote-extensionpack) extension in your VSCode
3. Get into the cloned directory and run `code .` to start up a VSCode session in there
4. Notice the notification at the right-bottom of your active editing window, click the `Reopen in Container` button or press `F1` to call up the command palette then find and click the item `Remote-container: Open folder in Container`
5. At this step VSCode will try to create&open a docker container for you according the `Dockerfile` in the root of this project. If you are first time in this step it will take some time to build the image, please pay a little patience to wait its completion. It will not do that rebuild next time if you did not change the `Dockerfile` in this project so later start up time will become somehow faster. 
6. The config file for VSCode remote development `.devcontainer/devcontainer.json` also includes some extensions for developing C/C++ in VSCode and they will be installed automatically in that container
7. Call out the command palettee and click `Cmake: Debug` to start an new debug session, you maybe required to select a build target, just select the `d8` if you're not sure.
8. The build process will start in the container, the completion time is according to how much the resource you allow the container to use, so if it's become too slow or you meet some weird errors like `[build] c++: internal compiler error: Killed (program cc1plus)` just increase the resource limitation and try again