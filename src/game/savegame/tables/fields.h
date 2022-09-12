/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2011 Yamagi Burmeister
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * Game fields to be saved.
 *
 * =======================================================================
 */

{"classname", static_cast<int>(FOFS(classname)), F_LSTRING},
{"model", static_cast<int>(FOFS(model)), F_LSTRING},
{"spawnflags", static_cast<int>(FOFS(spawnflags)), F_INT},
{"speed", static_cast<int>(FOFS(speed)), F_FLOAT},
{"accel", static_cast<int>(FOFS(accel)), F_FLOAT},
{"decel", static_cast<int>(FOFS(decel)), F_FLOAT},
{"target", static_cast<int>(FOFS(target)), F_LSTRING},
{"targetname", static_cast<int>(FOFS(targetname)), F_LSTRING},
{"pathtarget", static_cast<int>(FOFS(pathtarget)), F_LSTRING},
{"deathtarget", static_cast<int>(FOFS(deathtarget)), F_LSTRING},
{"killtarget", static_cast<int>(FOFS(killtarget)), F_LSTRING},
{"combattarget", static_cast<int>(FOFS(combattarget)), F_LSTRING},
{"message", static_cast<int>(FOFS(message)), F_LSTRING},
{"team", static_cast<int>(FOFS(team)), F_LSTRING},
{"wait", static_cast<int>(FOFS(wait)), F_FLOAT},
{"delay", static_cast<int>(FOFS(delay)), F_FLOAT},
{"random", static_cast<int>(FOFS(random)), F_FLOAT},
{"move_origin", static_cast<int>(FOFS(move_origin)), F_VECTOR},
{"move_angles", static_cast<int>(FOFS(move_angles)), F_VECTOR},
{"style", static_cast<int>(FOFS(style)), F_INT},
{"count", static_cast<int>(FOFS(count)), F_INT},
{"health", static_cast<int>(FOFS(health)), F_INT},
{"sounds", static_cast<int>(FOFS(sounds)), F_INT},
{"light", 0, F_IGNORE},
{"dmg", static_cast<int>(FOFS(dmg)), F_INT},
{"mass", static_cast<int>(FOFS(mass)), F_INT},
{"volume", static_cast<int>(FOFS(volume)), F_FLOAT},
{"attenuation", static_cast<int>(FOFS(attenuation)), F_FLOAT},
{"map", static_cast<int>(FOFS(map)), F_LSTRING},
{"origin", static_cast<int>(FOFS(s.origin)), F_VECTOR},
{"angles", static_cast<int>(FOFS(s.angles)), F_VECTOR},
{"angle", static_cast<int>(FOFS(s.angles)), F_ANGLEHACK},
{"goalentity", static_cast<int>(FOFS(goalentity)), F_EDICT, FFL_NOSPAWN},
{"movetarget", static_cast<int>(FOFS(movetarget)), F_EDICT, FFL_NOSPAWN},
{"enemy", static_cast<int>(FOFS(enemy)), F_EDICT, FFL_NOSPAWN},
{"oldenemy", static_cast<int>(FOFS(oldenemy)), F_EDICT, FFL_NOSPAWN},
{"activator", static_cast<int>(FOFS(activator)), F_EDICT, FFL_NOSPAWN},
{"groundentity", static_cast<int>(FOFS(groundentity)), F_EDICT, FFL_NOSPAWN},
{"teamchain", static_cast<int>(FOFS(teamchain)), F_EDICT, FFL_NOSPAWN},
{"teammaster", static_cast<int>(FOFS(teammaster)), F_EDICT, FFL_NOSPAWN},
{"owner", static_cast<int>(FOFS(owner)), F_EDICT, FFL_NOSPAWN},
{"mynoise", static_cast<int>(FOFS(mynoise)), F_EDICT, FFL_NOSPAWN},
{"mynoise2", static_cast<int>(FOFS(mynoise2)), F_EDICT, FFL_NOSPAWN},
{"target_ent", static_cast<int>(FOFS(target_ent)), F_EDICT, FFL_NOSPAWN},
{"chain", static_cast<int>(FOFS(chain)), F_EDICT, FFL_NOSPAWN},
{"prethink", static_cast<int>(FOFS(prethink)), F_FUNCTION, FFL_NOSPAWN},
{"think", static_cast<int>(FOFS(think)), F_FUNCTION, FFL_NOSPAWN},
{"blocked", static_cast<int>(FOFS(blocked)), F_FUNCTION, FFL_NOSPAWN},
{"touch", static_cast<int>(FOFS(touch)), F_FUNCTION, FFL_NOSPAWN},
{"use", static_cast<int>(FOFS(use)), F_FUNCTION, FFL_NOSPAWN},
{"pain", static_cast<int>(FOFS(pain)), F_FUNCTION, FFL_NOSPAWN},
{"die", static_cast<int>(FOFS(die)), F_FUNCTION, FFL_NOSPAWN},
{"stand", static_cast<int>(FOFS(monsterinfo.stand)), F_FUNCTION, FFL_NOSPAWN},
{"idle", static_cast<int>(FOFS(monsterinfo.idle)), F_FUNCTION, FFL_NOSPAWN},
{"search", static_cast<int>(FOFS(monsterinfo.search)), F_FUNCTION, FFL_NOSPAWN},
{"walk", static_cast<int>(FOFS(monsterinfo.walk)), F_FUNCTION, FFL_NOSPAWN},
{"run", static_cast<int>(FOFS(monsterinfo.run)), F_FUNCTION, FFL_NOSPAWN},
{"dodge", static_cast<int>(FOFS(monsterinfo.dodge)), F_FUNCTION, FFL_NOSPAWN},
{"attack", static_cast<int>(FOFS(monsterinfo.attack)), F_FUNCTION, FFL_NOSPAWN},
{"melee", static_cast<int>(FOFS(monsterinfo.melee)), F_FUNCTION, FFL_NOSPAWN},
{"sight", static_cast<int>(FOFS(monsterinfo.sight)), F_FUNCTION, FFL_NOSPAWN},
{"checkattack", static_cast<int>(FOFS(monsterinfo.checkattack)), F_FUNCTION, FFL_NOSPAWN},
{"currentmove", static_cast<int>(FOFS(monsterinfo.currentmove)), F_MMOVE, FFL_NOSPAWN},
{"endfunc", static_cast<int>(FOFS(moveinfo.endfunc)), F_FUNCTION, FFL_NOSPAWN},
{"lip", static_cast<int>(STOFS(lip)), F_INT, FFL_SPAWNTEMP},
{"distance", static_cast<int>(STOFS(distance)), F_INT, FFL_SPAWNTEMP},
{"height", static_cast<int>(STOFS(height)), F_INT, FFL_SPAWNTEMP},
{"noise", static_cast<int>(STOFS(noise)), F_LSTRING, FFL_SPAWNTEMP},
{"pausetime", static_cast<int>(STOFS(pausetime)), F_FLOAT, FFL_SPAWNTEMP},
{"item", static_cast<int>(STOFS(item)), F_LSTRING, FFL_SPAWNTEMP},
{"item", static_cast<int>(FOFS(item)), F_ITEM},
{"gravity", static_cast<int>(STOFS(gravity)), F_LSTRING, FFL_SPAWNTEMP},
{"sky", static_cast<int>(STOFS(sky)), F_LSTRING, FFL_SPAWNTEMP},
{"skyrotate", static_cast<int>(STOFS(skyrotate)), F_FLOAT, FFL_SPAWNTEMP},
{"skyaxis", static_cast<int>(STOFS(skyaxis)), F_VECTOR, FFL_SPAWNTEMP},
{"minyaw", static_cast<int>(STOFS(minyaw)), F_FLOAT, FFL_SPAWNTEMP},
{"maxyaw", static_cast<int>(STOFS(maxyaw)), F_FLOAT, FFL_SPAWNTEMP},
{"minpitch", static_cast<int>(STOFS(minpitch)), F_FLOAT, FFL_SPAWNTEMP},
{"maxpitch", static_cast<int>(STOFS(maxpitch)), F_FLOAT, FFL_SPAWNTEMP},
{"nextmap", static_cast<int>(STOFS(nextmap)), F_LSTRING, FFL_SPAWNTEMP},
{0, 0, static_cast<fieldtype_t>(0), 0}
