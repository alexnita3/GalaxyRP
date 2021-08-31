# Version 3.3
Version 3.3 is here! _Everything is saved now?_

- Added 31 new animations to use with /emote
- Everything will now save in a database, no more fiddling with text files
- Added /resetpassword
- Model and nicknames will now be saved and loaded per character from the database. (Once you change it make sure to /logout for it to save!)
- Both /anim and /emote work the same now
- Both /inv and /inventory work the same now
- Added /scale help which displays the ig scale compared to real life measurements
- Removed golden aura when dueling
- When dueling, other players will not be visible anymore
- /emote list is now more ordered, with categories in which the animations are sorted
- Tavion and Desann (Purple and Green styles) are now properly balanced
- Added autoWalk

FOR PLAYERS: To bind an autowalk button do the following in your console:

`seta autoWalkOn "+forward;bind key vstr autoWalkOff"`\
`seta autoWalkOff "-forward;bind key vstr autoWalkOn"`\
`bind key vstr autoWalkOn`\
Replace "key" with your desired button.

FOR ADMINS: Delete the existing GalaxyRP folder from the server, it contains unused folders. (don't forget to back up). It comes with a default account of user: `admin` pass: `admin`. So don't forget to /resetpassword.

Special thanks to JustJordyn, RepJunkie, TriForce and ZelZel for helping out with stuff I had no knowledge about!

# Version 3.2
Version 3.2 is here! _Let me check my inventory..._

- Added /mylow, /myall, /mylong, /my chat modifiers.
- Added a brand new inventory system, with custom items that can be created (/createitem), deleted (/trashitem), and passed around between players (/giveitem).
- Added a brand new description system for characters. Players can now set their own character descriptions (/attributes), which can be seen by other players (/examine).
- Added six more admin permissions that allow the use of their respective commands. (fully backwards compatible)
- Added /flipcoin command.
- Fixed a chat bug where text in the middle of the string would be interpreted as a command. (e.g. "I know /all/ the cool places." would be interpreted as /all. Not anymore!)
- Fixed dueling always starting people at 100 hp 100 shield.

# Version 3.1
Version 3.1 is here! _Haven't you read the news yet?_

- /all is now fixed, and broadcasts to the whole server.
- Added a news system in-game, where news can be read and written ICly.
- AdminProtect will now allow admins to see all chat on the server, to allow them to monitor players.
- Minor bugfix with /all where the message wouldn't be displayed correctly on the screen (missing the colon)

# Version 3.0
Version 3.0 is here! _I can do what with the chat now??_

- Chat system revamped, added chat modifiers for all actions that can be done. The normal chat is unchanged should players prefer to keep using that.
- Entity saving is now fixed, no longer crashes the server. (whoops)
- /roll will never display 0 anymore. It was due to a bug in the random number generator. (whoops x2)

PRO TIP: The server side and local server now come with an admin account by default. Username is admin and password is admin.
FOR ALL THAT IS SACRED CHANGE THE ACCOUNT DETAILS AFTER YOU START HOSING ONLINE!

# Version 2.0
Version 2.0 is finally here! _What do you mean i have to pay more for stuff now?_

The following has been changed:

- /god now works for everyone in the server, and will be broadcast to let everyone know. This is to allow players with less skill to take part in combat and be able to roleplay properly.
- /rpmodeup and /rpmodedown are replaced by /skillup and /skilldown
- /createcredits is now a separate command (but it can still only be used by admins)
- /creditgive is now replaced by /givecredits
- /spendcredits was added, which broadcasts a message and takes the money form the players account.
- .sab variable is now increased, hopefully allowing for more saber models to be added.
- Weapons will now have to be bought IG, and an admin has to authorize it.
- Store prices have been balanced, and increased to better allow lower denominations.
- Unused items have been removed form the shop.
- Added a /news command, used by players to get IC news updates from the server. This can either be /news, /news republic, /news sith and /news jedi.
- Credit creation is now only allowed by admins with GiveAdmin permission.
- Some sounds have been disabled, namely see, distract, distractstop, absorbloop and seeloop. This is for better immersion.
- Added 16 new animations, be sure to check out /emote list to find them.
- /list rpg now replaced by /list help

# Version 1.1.1 - bugfix
For now, weapons will work like this:

- No need to BUY the weapon itself, everyone will have all weapons from the start (but no ammo)
- Ammo is saved and if you run out you'll have to buy more form the seller if you want to use the weapon.

I have a proper fix in mind, but it might require massive rework in the way accounts are saved (which I planned to do anyway) so it will take time before I add it.
Sorry about that!

# Version 1.1
Version 1.1 is finally here! _Rolls a natural 20_
The following has been changed:

- Ammo files aren't shown as characters anymore
- Sniper battle minigame now works
- Melee battle minigame now works
- Added some validation to the name of characters (special characters apart from "_" are not allowed anymore)
- Dueling now works just like in ja+
- Duel tournament now works
- Last Man Standing now works
- "Magic Potion" doesn't show up anymore at the item seller (Duh!)
- Added dice roll command (/roll <max_roll>)
- Map loading screen is now less cluttered, leaving more room for the artwork.

This version is also generally more robust, I haven't been able to break it. (yet!)

(Update: Forgot to compile the right version so if you see this and you didn't when you got it, get it again please)

# Version 1.0
Version 1.0 is finally here!
Full list of features that are included.

- Account system
- Ammo and skills are saved per character
- Fully working shop
- Credit system (Admins can now create credits)
- OOC chat
- NoClip now works as intended
- Custom UI
- Custom HUD
- Fully working emote system words AND IDs (Do /emote list to see everything)
- Desann and Tavion are unlocked
- New Upgrades for Mercs as well as more force powers for Force sensitive chars
- Added scripts for setting resolution
- Admin permissions work as intended
- Voice chat works as intended (Not mic but pre recorded commands)
- Weapons upgrades work as intended
- Podracing now works
- Ally system now works
- Entity system now works
- Shader remapping through ig commands now works
- Skills that can be added or removed by admins only
- Broadcasting of a player's login/logout message to let people know what they logged in as.

This is the first stable version, and as such, I'm releasing it to the public, the feature pool is big enough that I feel like it can finally be used to start an RP community.

# Ready for testing
The following functionality is included:

- Advanced admin system
- Advanced credit system
- More abilities can be unlocked compared to in the base game
- New Art
- Improved UI