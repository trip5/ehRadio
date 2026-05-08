# How to Sync Config Files in VS Code

This folder contains contributor-specific configuration files that are automatically synced from the root directory

## What Gets Synced
  - /platformio.ini
  - /myoptions.h
  - /mytheme.h

## Setup

### Create your builds folder
  - Create a subfolder in builds with your Github username

### Install File Watcher Extension
1. Open VS Code Extensions (Ctrl+Shift+X)
2. Search for: appulate.filewatcher
3. Install "File Watcher" by appulate


### Edit .vscode/settings.json
  - Add a section that looks like (edit username):
   ```json
	"filewatcher.isClearStatusBar": true,
	"filewatcher.statusBarDelay": 3000,
	"filewatcher.commands": [
		{
			"match": "platformio.ini",
			"cmd": "if \"${file}\"==\"${workspaceRoot}\\platformio.ini\" copy \"${file}\" \"${workspaceRoot}\\builds\\username\\platformio.ini\"", 
			"event": "onFileChange"
		},
		{
			"match": "myoptions.h",
			"cmd": "if \"${file}\"==\"${workspaceRoot}\\myoptions.h\" copy \"${file}\" \"${workspaceRoot}\\builds\\username\\myoptions.h\"", 
			"event": "onFileChange"
		},
		{
			"match": "mytheme.h",
			"cmd": "if \"${file}\"==\"${workspaceRoot}\\mytheme.h\" copy \"${file}\" \"${workspaceRoot}\\builds\\username\\mytheme.h\"", 
			"event": "onFileChange"
		}
	]
   ```
  - don't forget it's JSON so brackets and commas are important!
  - see the `settings.json` for an example

### Done!

The 3 important files that are ignored by `.gitignore` will now be backed-up and shared and Github workflow will build your firmwares automatically!

### Note

I could not find a way to elegantly match the files in the root of the workspace folder so it matches an exact filename but uses an if in the cmd.

If you use powershell or bash... please edit and share your `settings.json` in your builds folder after you get it working!
