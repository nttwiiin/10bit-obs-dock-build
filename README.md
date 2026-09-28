# 10BIT Broadcast OBS Dock — GitHub build-ready source

Native Qt/OBS Frontend API dock. No Browser Dock and no overlay URL is used for control.

## Target

- Windows x64
- OBS Studio 32.x target machine
- Dock API: \`obs_frontend_add_dock_by_id\`
- Core link: authenticated local TCP JSON bridge exposed by 10BIT Broadcast

## CI build

The GitHub Actions workflow clones the official \`obsproject/obs-plugintemplate\`, overlays this plugin source, enables Qt + OBS Frontend API, builds on \`windows-2022\`, then uploads \`10BIT_OBS_DOCK_WINDOWS_X64.zip\`.

The initial CI baseline uses the current official plugin-template dependency set (OBS 31.1.1 SDK / Qt package) for a compatibility build. The resulting DLL is intended to be tested on the current OBS 32.2.2 workstation before it is embedded into 10BIT Broadcast.

## Expected install layout for OBS 32

\`\`\`text
C:\ProgramData\obs-studio\plugins\10bit-broadcast-dock\
├── bin\64bit\10bit-broadcast-dock.dll
└── data\locale\
    ├── en-US.ini
    └── vi-VN.ini
\`\`\`

Restart OBS after installation, then check \`Docks > 10BIT Broadcast\`.
