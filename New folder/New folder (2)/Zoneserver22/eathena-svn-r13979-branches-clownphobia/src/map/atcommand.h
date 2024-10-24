// $Id: atcommand.h 148 2004-09-30 14:05:37Z MouseJstr $
#ifndef _ATCOMMAND_H_
#define _ATCOMMAND_H_

enum AtCommandType {
	AtCommand_None = -1,
	AtCommand_Broadcast = 0,
	AtCommand_LocalBroadcast,
	AtCommand_MapMove,
	AtCommand_ResetState,
	AtCommand_RuraP,
	AtCommand_Rura,
	AtCommand_Warp,
	AtCommand_Where,
	AtCommand_JumpTo,
	AtCommand_Jump,
	AtCommand_Who,
	AtCommand_Who2,
	AtCommand_Who3,
	AtCommand_WhoMap,
	AtCommand_WhoMap2,
	AtCommand_WhoMap3,
	AtCommand_WhoGM,
	AtCommand_Save,
	AtCommand_Load,
	AtCommand_Speed,
	AtCommand_Storage,
	AtCommand_GuildStorage,
	AtCommand_Option,
	AtCommand_Hide,
	AtCommand_JobChange,
	AtCommand_JobChange2,
	AtCommand_JobChange3,
	AtCommand_Die,
	AtCommand_Kill,
	AtCommand_Alive,
	AtCommand_Kami,
	AtCommand_KamiB,
	AtCommand_Heal,
	AtCommand_Item,
	AtCommand_Item2,
	AtCommand_ItemReset,
	AtCommand_ItemCheck,
	AtCommand_BaseLevelUp,
	AtCommand_JobLevelUp,
	AtCommand_H,
	AtCommand_Help,
	AtCommand_GM,
	AtCommand_PvPOff,
	AtCommand_PvPOn,
	AtCommand_GvGOff,
	AtCommand_GvGOn,
	AtCommand_Model,
	AtCommand_Go,
	AtCommand_Spawn,
	AtCommand_Monster,
	AtCommand_MonsterSmall,
	AtCommand_MonsterBig,
	AtCommand_KillMonster,
	AtCommand_KillMonster2,
	AtCommand_Refine,
	AtCommand_Produce,
	AtCommand_Memo,
	AtCommand_GAT,
	AtCommand_Packet,
	AtCommand_StatusPoint,
	AtCommand_SkillPoint,
	AtCommand_Zeny,
	AtCommand_Param,
	AtCommand_Strength,
	AtCommand_Agility,
	AtCommand_Vitality,
	AtCommand_Intelligence,
	AtCommand_Dexterity,
	AtCommand_Luck,
	AtCommand_GuildLevelUp,
	AtCommand_MakeEgg,
	AtCommand_PetFriendly,
	AtCommand_PetHungry,
	AtCommand_PetRename,
//	AtCommand_CharPetRename, // by Yor, Now #petrename
	AtCommand_Recall,
//	AtCommand_CharacterJob,  // Now #jobchange
	AtCommand_Revive,
//	AtCommand_CharacterStats, // Now #stats
	AtCommand_CharacterStatsAll,
//	AtCommand_CharacterOption, // Now #option
//	AtCommand_CharacterSave, // Now #save
	AtCommand_CharacterLoad,
	AtCommand_Night,
	AtCommand_Day,
	AtCommand_Doom,
	AtCommand_DoomMap,
	AtCommand_Raise,
	AtCommand_RaiseMap,
	AtCommand_CharacterBaseLevel,
	AtCommand_CharacterJobLevel,
	AtCommand_Kick,
	AtCommand_KickAll,
	AtCommand_AllSkill,
	AtCommand_QuestSkill,
	AtCommand_CharQuestSkill,
	AtCommand_LostSkill,
	AtCommand_CharLostSkill,
	AtCommand_SpiritBall,
	AtCommand_Party,
	AtCommand_Guild,
	AtCommand_AgitStart,
	AtCommand_AgitEnd,
	AtCommand_MapExit,
	AtCommand_IDSearch,
	AtCommand_CharSkReset,
	AtCommand_CharStReset,
	//by chbrules
	AtCommand_CharModel,
	AtCommand_CharSKPoint,
	AtCommand_CharSTPoint,
//	AtCommand_CharZeny, //now #zeny
	AtCommand_RecallAll,
	AtCommand_ReloadItemDB,
	AtCommand_ReloadMobDB,
	AtCommand_ReloadSkillDB,
	AtCommand_ReloadScript,
	AtCommand_ReloadGMDB,
	AtCommand_ReloadAtcommand,
	AtCommand_ReloadBattleConf,
	AtCommand_ReloadStatusDB,
	AtCommand_ReloadPcDB,
	AtCommand_MapInfo,
	AtCommand_Dye,
	AtCommand_Hstyle,
	AtCommand_Hcolor,
	AtCommand_StatAll,
	AtCommand_CharChangeSex, // by Yor
	AtCommand_CharBlock, // by Yor
	AtCommand_CharBan, // by Yor
	AtCommand_CharUnBlock, // by Yor
	AtCommand_CharUnBan, // by Yor
	AtCommand_MountPeco, // by Valaris
	AtCommand_CharMountPeco, // by Yor
	AtCommand_GuildSpy, // [Syrus22]
	AtCommand_PartySpy, // [Syrus22]
	AtCommand_RepairAll, // [Valaris]
	AtCommand_GuildRecall, // by Yor
	AtCommand_PartyRecall, // by Yor
	AtCommand_Nuke,	// [Valaris]
	AtCommand_Shownpc,
	AtCommand_Hidenpc,
	AtCommand_Loadnpc,
	AtCommand_Unloadnpc,
	AtCommand_ServerTime, // by Yor
	AtCommand_CharDelItem, // by Yor
	AtCommand_Jail, // by Yor
	AtCommand_UnJail, // by Yor
	AtCommand_Disguise, // [Valaris]
	AtCommand_Disguise2, // [Valaris]
	AtCommand_UnDisguise, // by Yor
	AtCommand_CharDisguise, // Kalaspuff
	AtCommand_CharUnDisguise, // Kalaspuff
	AtCommand_EMail, // by Yor
	AtCommand_Hatch,
	AtCommand_Effect, // by Apple
// 	AtCommand_Char_Item_List, // by Yor, now #itemlist
//	AtCommand_Char_Storage_List, // by Yor, now #storagelist
	AtCommand_Char_Cart_List, // by Yor
	AtCommand_AddWarp, // by MouseJstr
	AtCommand_Follow, // by MouseJstr
	AtCommand_SkillOn, // by MouseJstr
	AtCommand_SkillOff, // by MouseJstr
	AtCommand_Killer, // by MouseJstr
	AtCommand_NpcMove, // by MouseJstr
	AtCommand_Killable, // by MouseJstr
	AtCommand_CharKillable, // by MouseJstr
//	AtCommand_Chareffect, // by MouseJstr, now #effect
	AtCommand_Dropall, // by MouseJstr
	AtCommand_Chardropall, // by MouseJstr
	AtCommand_Storeall, // by MouseJstr
	AtCommand_Charstoreall, // by MouseJstr
	AtCommand_Skillid, // by MouseJstr
	AtCommand_Useskill, // by MouseJstr
	AtCommand_Summon,
	AtCommand_Rain,
	AtCommand_Snow,
	AtCommand_Sakura,
	AtCommand_Clouds,
	AtCommand_Fog,
	AtCommand_Fireworks,
	AtCommand_Leaves,
	AtCommand_AdjGmLvl, // MouseJstr
	AtCommand_AdjCmdLvl, // MouseJstr
	AtCommand_Trade, // MouseJstr
	AtCommand_Send,
	AtCommand_SetBattleFlag,
	AtCommand_UnMute,
	AtCommand_Clearweather, // by Dexity
	AtCommand_UpTime, // by MC Cameri
	AtCommand_ChangeSex, // by MC Cameri
	AtCommand_Mute, // [celest]
	AtCommand_WhoZeny, // [Valaris] <-- LOL...(MC Cameri) worth it.
	AtCommand_HappyHappyJoyJoy, // [Valaris]
	AtCommand_Refresh, // by MC Cameri
	AtCommand_PetId, // by MC Cameri
	AtCommand_Identify, // by MC Cameri
	AtCommand_Gmotd, // Added by MC Cameri, created by davidsiaw
	AtCommand_MiscEffect, // by MC Cameri
	AtCommand_MobSearch,
	AtCommand_CleanMap,
	AtCommand_NpcTalk,
	AtCommand_PetTalk,
	AtCommand_Users,
	AtCommand_StatusUp,
	// SQL-only commands start
#ifdef MAIL
	AtCommand_CheckMail, // [Valaris]
	AtCommand_ListMail, // [Valaris]
	AtCommand_ListNewMail, // [Valaris]
	AtCommand_ReadMail, // [Valaris]
	AtCommand_SendMail, // [Valaris]
	AtCommand_DeleteMail, // [Valaris]
	AtCommand_SendPriorityMail, // [Valaris]
#endif
//	AtCommand_Sound, // [Valaris]
#ifndef TXT_ONLY
	AtCommand_RefreshOnline, // [Valaris]
	// SQL-only commands end
#endif
	AtCommand_SkillTree, // by MouseJstr
	AtCommand_Marry, // by MouseJstr
	AtCommand_Divorce, // by MouseJstr
	AtCommand_Grind, // by MouseJstr
	AtCommand_Grind2, // by MouseJstr

	AtCommand_DMStart, // by MouseJstr
	AtCommand_DMTick, // by MouseJstr

	AtCommand_JumpToId, // by Dino9021
	AtCommand_JumpToId2, // by Dino9021
	AtCommand_RecallId, // by Dino9021
	AtCommand_RecallId2, // by Dino9021
	AtCommand_KickId, // by Dino9021
	AtCommand_KickId2, // by Dino9021
	AtCommand_ReviveId, // by Dino9021
	AtCommand_ReviveId2, // by Dino9021
	AtCommand_KillId, // by Dino9021
	AtCommand_KillId2, // by Dino9021
	AtCommand_CharKillableId, // by Dino9021
	AtCommand_CharKillableId2, // by Dino9021
	AtCommand_Sound,
	AtCommand_UndisguiseAll,
	AtCommand_DisguiseAll,
	AtCommand_ChangeLook,
	AtCommand_AutoLoot, //by Upa-Kun
	AtCommand_MobInfo, //by Lupus
        AtCommand_Adopt, // by Veider
        AtCommand_Version, // by Ancyker

        AtCommand_MuteArea, // MouseJstr
        AtCommand_Shuffle, // MouseJstr
        AtCommand_Rates, // MouseJstr

	AtCommand_ItemInfo, // Lupus
	AtCommand_MapFlag, // Lupus
	AtCommand_GMb,	// Aru
	AtCommand_Ice,	// Aru
	AtCommand_UnIce,	//Aru
	AtCommand_God,	//Aru
	AtCommand_Break,	//Aru
	AtCommand_Pet16,
	AtCommand_FakeSC,

	// end
	AtCommand_Unknown,
	AtCommand_MAX
};

typedef enum AtCommandType AtCommandType;

typedef struct AtCommandInfo {
	AtCommandType type;
	const char* command;
	int level;
	int (*proc)(const int, struct map_session_data*,
		const char* command, const char* message);
} AtCommandInfo;

AtCommandType
is_atcommand(const int fd, struct map_session_data* sd, const char* message, int gmlvl);

AtCommandType atcommand(
	struct map_session_data *sd,
	const int level, const char* message, AtCommandInfo* info);
int get_atcommand_level(const AtCommandType type);

char * msg_txt(int msg_number); // [Yor]

int atcommand_item(const int fd, struct map_session_data* sd,const char* command, const char* message); // [Valaris]
int atcommand_rura(const int fd, struct map_session_data* sd,const char* command, const char* message); // [Yor]
int atcommand_spawn(const int fd, struct map_session_data* sd, const char* command, const char* message); // [Valaris]
int atcommand_jumpto(const int fd, struct map_session_data* sd, const char* command, const char* message); // [Yor]
int atcommand_recall(const int fd, struct map_session_data* sd, const char* command, const char* message); // [Yor]

int atcommand_config_read(const char *cfgName);
int msg_config_read(const char *cfgName);
void do_final_msg();

char *estr_lower(char *str);

char * job_name(int class_);
int e_mail_check(char *email);

#define MAX_MSG 1000
extern char *msg_table[MAX_MSG];

#endif

#define ATCOMMAND_FUNC() ACMD_FUNC()

