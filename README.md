## vSID

vSID is a powerful plugin for Euroscope that assists controllers with SID selection, clearances and some additional features.

The plugin is used within several vACCs by default.

## Feature List
* Configuration files for great variation and different conditions. For details check the Github wiki to find out the huge potential of the plugin.
* SID/RWY suggestion based on the config file and the filed flight plan.
* Initial climb suggestion based on the SID.
* Different colors for SID, climb, runway to indicate any deviations from default assigment and highlights.
* Optimized menus für SIDs, runways, climbs, etc.
* Request menu for different requests as clearance, startup, pushback, taxi, vfr.
* Startup counter per runway (all "active" flights with startup approval).
* Intersection menu to set pre-defined or custom runway intersections as "cleared" or "able for" (also available as text entry in the scratch pad - see the wiki for details)
* Optional pushback, request and intersection indicator next to the radar target.

## Default colors
* SID
  * suggested - white
  * custom suggestion - yellow
  * set - green
  * custom set - orange
  * error - red
* Initial climb
  * suggested - white
  * set climb - cyan
  * set climb via - green
  * set custom - orange
* Runway
  * suggested - white
  * set - green
  * set, no dep rwy - yellow

**Coloring examples**

![image](https://github.com/user-attachments/assets/0fc232ac-dd57-4e1f-ae6b-d4c778abf954)

**Menu example**

![image](https://github.com/user-attachments/assets/1c8e2d02-44e2-4676-aeda-410ffcddcb92)

## Help and Support
If you need any help or support setting up the config for your airport (vACC), feel free to open an issue.

## Wine / Linux users
Under Wine the ICU-backed `std::chrono` time zone database does not work, so vSID reads the IANA time zone data (TZif files) itself.
Every release and pre-release therefore ships a `zoneinfo.zip` next to `files.zip` and `config.zip`.

1. Download `zoneinfo.zip` from the same release as `vSID.dll`.
2. Unzip it next to `vSID.dll`, so that the folder `zoneinfo` (containing `Europe/Berlin`, ... and `+VERSION`) sits in the same directory as the DLL.
3. Optionally set a different folder name with the key `zoneinfoPath` in `vSidConfig.json` (default: `zoneinfo`; relative to the DLL directory).

Windows users do not need this folder; it is never read on Windows.
If the folder is missing on Wine, vSID reports an error and falls back to UTC.

Wine is detected automatically. If vSID does not detect your Wine setup (the log line `Time provider [...] - Wine detected [NO]` at start-up), set `"timeProvider": "wine"` in `vSidConfig.json`.
Allowed values: `auto` (default, automatic detection), `wine` (always use the `zoneinfo` folder), `windows` (always use the Windows time zone database).
Both `timeProvider` and `zoneinfoPath` are re-read by `.vsid reload`.

## Building / pinned tz data
The time zone data is not committed as binary files. The release workflow compiles it from the upstream sources
(https://github.com/eggert/tz), which are linked as the git submodule `external/tz` and pinned to a release tag (the pin is the submodule commit recorded in this repository).

Clone including the submodule:

```
git clone --recurse-submodules <repo-url>
```

or, in an existing clone:

```
git submodule update --init
```

The Windows plugin build itself does not need the submodule.

### Updating the pinned tz version
```
git -C external/tz fetch --tags
git -C external/tz checkout <tag>     # e.g. a newer release tag such as 2026g
git add external/tz
git commit
```

The `Zoneinfo` workflow (`.github/workflows/zoneinfo.yml`) caches the compiled data by pin where the cache is available (release builds run on `pull_request` close events, so caches saved on PR merge refs may not be shared), so a new `zic` run normally only happens after the pin changed. The workflow fails if the pinned commit is not exactly a release tag.
