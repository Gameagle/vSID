# vSID

vSID is a small plugin for Euroscope that aims to help controllers with SID selection and clearances. There are some more small features to aid in the management of an airport.

### Feature List
* Configuration files for great variation
  * Values that can be configured are `aircraft type`, `engine count`, `engine type`, `MTOW`, `rnav / non-rnav aircraft`, `runways`, `initial climb`, `custom rule`, `areas`, `priority`
* SID suggestion based on the config file and the filed flight plan
  * Different colors for `suggested`, `suggested based on different runway`, `flight plan matches suggestion`, `flight plan does NOT match suggestion (custom)`, `error`
* Initial climb suggestion based on the suggested SID
  * Different colors for `suggested`, `set and climb`, `set and climb via` and `set and custom altitude (not matching the SID or for VFR traffic)`
* Runway suggestion based on the suggested SID
  * Different colors for `suggested`, `set`, `set but not a departure runway`
* Request menu for different requests as `clearance`, `startup`, `pushback`, `taxi`, `vfr`
* Custom menu with a submenu for each airport
  * Startup counter menu (currently for all active departure runways)
  * More menues coming later
* Areas can be configured with Euroscope coordinates to "draw" an area around different stands or parts of the airport that can then be used to configure SIDs (SID is allowed/denied based on wether the area is active and the aircraft is inside this area or not)
* Custom rules that can be set to be used to further configure SIDs (basically a rule name that is than set in a SID to allow/deny this SID based on wether the rule is active or not)

### Default colors
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

