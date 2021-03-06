#include "Globals.h"
#include "PluginImpl.h"
#include <fcntl.h>
#include <elf.h>
#include <sudo.h>

SHAREDSYMBOL void WINPORT_DllStartup(const char *path)
{
	G.plugin_path = path;
//	fprintf(stderr, "ObjInfo::WINPORT_DllStartup\n");
}

SHAREDSYMBOL int WINAPI _export GetMinFarVersion(void)
{
	#define MAKEFARVERSION(major,minor) ( ((major)<<16) | (minor))
	return MAKEFARVERSION(2, 1);
}


SHAREDSYMBOL void WINAPI _export SetStartupInfo(const struct PluginStartupInfo *Info)
{
	G.Startup(Info);
//	fprintf(stderr, "ObjInfo::SetStartupInfo\n");
}

#define MINIMAL_LEN	0x34


SHAREDSYMBOL HANDLE WINAPI _export OpenFilePlugin(const char *Name, const unsigned char *Data, int DataSize, int OpMode)
{
	if (!G.IsStarted())
		return INVALID_HANDLE_VALUE;

	//FAR reads at least 0x1000 bytes if possible, so may assume full ELF header available if its there
	if (DataSize < MINIMAL_LEN || Data[0] != 0x7F || Data[1] != 'E'|| Data[2] != 'L'|| Data[3] != 'F')
		return INVALID_HANDLE_VALUE;
	if (Data[4] != 1 && Data[4] != 2) // Bitness: 32/64
		return INVALID_HANDLE_VALUE;
	if (Data[5] != 0 && Data[5] != 1 && Data[5] != 2) // Endianess: None(?)/LSB/MSB
		return INVALID_HANDLE_VALUE;
	if (Data[6] != 1) // Version: 1
		return INVALID_HANDLE_VALUE;

	// Well, it really looks like valid ELF file
	// If used called us with Ctrl+PgDn - then proceed
	// Otherwise check if file is eXecutable and proceed only if its not, to allow user execute it by Enter
	if ((OpMode & (OPM_PGDN|OPM_COMMANDS)) == 0) {
		struct stat s = {};
		if (sdc_stat(Name, &s) == -1) // in case of any uncertainlity...
			return INVALID_HANDLE_VALUE;
		if ((s.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0)
			return INVALID_HANDLE_VALUE;
	}

	PluginImpl *out = new PluginImpl(Name, Data[4], Data[5]);
	return out ? (HANDLE)out : INVALID_HANDLE_VALUE;
}


SHAREDSYMBOL HANDLE WINAPI _export OpenPlugin(int OpenFrom, INT_PTR Item)
{
	if (!G.IsStarted() || OpenFrom != OPEN_COMMANDLINE)
		return INVALID_HANDLE_VALUE;

	if (strncmp((const char *)Item, G.command_prefix.c_str(), G.command_prefix.size()) != 0
	||  ((const char *)Item)[G.command_prefix.size()] != ':') {
		return INVALID_HANDLE_VALUE;
	}

	std::string path( &((const char *)Item)[G.command_prefix.size() + 1] );
	if (path.size() >1 && path[0] == '\"' && path[path.size() - 1] == '\"')
		path = path.substr(1, path.size() - 2);
	if (path.empty())
		return INVALID_HANDLE_VALUE;

	int fd = sdc_open(path.c_str(), O_RDONLY);
	if (fd == -1)
		return INVALID_HANDLE_VALUE;

	unsigned char data[MINIMAL_LEN];
	int data_len = sdc_read(fd, data, sizeof(data));
	sdc_close(fd);
	return OpenFilePlugin(path.c_str(), data, data_len, OPM_COMMANDS);
}

SHAREDSYMBOL void WINAPI _export ClosePlugin(HANDLE hPlugin)
{
	delete (PluginImpl *)hPlugin;
}


SHAREDSYMBOL int WINAPI _export GetFindData(HANDLE hPlugin,struct PluginPanelItem **pPanelItem,int *pItemsNumber,int OpMode)
{
	return ((PluginImpl *)hPlugin)->GetFindData(pPanelItem, pItemsNumber, OpMode);
}


SHAREDSYMBOL void WINAPI _export FreeFindData(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber)
{
	((PluginImpl *)hPlugin)->FreeFindData(PanelItem, ItemsNumber);
}


SHAREDSYMBOL int WINAPI _export SetDirectory(HANDLE hPlugin,const char *Dir,int OpMode)
{
 	return ((PluginImpl *)hPlugin)->SetDirectory(Dir, OpMode);
}


SHAREDSYMBOL int WINAPI _export DeleteFiles(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber,int OpMode)
{
	return ((PluginImpl *)hPlugin)->DeleteFiles(PanelItem, ItemsNumber, OpMode);
}


SHAREDSYMBOL int WINAPI _export GetFiles(HANDLE hPlugin,struct PluginPanelItem *PanelItem,
                   int ItemsNumber,int Move,char *DestPath,int OpMode)
{
	return ((PluginImpl *)hPlugin)->GetFiles(PanelItem, ItemsNumber, Move, DestPath, OpMode);
}


SHAREDSYMBOL int WINAPI _export PutFiles(HANDLE hPlugin,struct PluginPanelItem *PanelItem,
                   int ItemsNumber,int Move,int OpMode)
{
	return ((PluginImpl *)hPlugin)->PutFiles(PanelItem, ItemsNumber, Move, OpMode);
}


SHAREDSYMBOL void WINAPI _export ExitFAR()
{
}


SHAREDSYMBOL void WINAPI _export GetPluginInfo(struct PluginInfo *Info)
{
	Info->StructSize = sizeof(*Info);

	Info->Flags = PF_FULLCMDLINE;
	static const char *PluginCfgStrings[1];
	PluginCfgStrings[0] = (char*)G.GetMsg(MCfgLine0);
	Info->PluginConfigStrings = PluginCfgStrings;
	Info->PluginConfigStringsNumber = ARRAYSIZE(PluginCfgStrings);

	static char s_command_prefix[G.MAX_COMMAND_PREFIX + 1] = {}; // WHY?
	strncpy(s_command_prefix, G.command_prefix.c_str(), sizeof(s_command_prefix));
	Info->CommandPrefix = s_command_prefix;
}

SHAREDSYMBOL void WINAPI _export GetOpenPluginInfo(HANDLE hPlugin,struct OpenPluginInfo *Info)
{
	((PluginImpl *)hPlugin)->GetOpenPluginInfo(Info);
}


SHAREDSYMBOL int WINAPI _export ProcessHostFile(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber,int OpMode)
{
  return ((PluginImpl *)hPlugin)->ProcessHostFile(PanelItem, ItemsNumber, OpMode);
}

SHAREDSYMBOL int WINAPI _export ProcessKey(HANDLE hPlugin,int Key,unsigned int ControlState)
{
	return ((PluginImpl *)hPlugin)->ProcessKey(Key, ControlState);
}

SHAREDSYMBOL int WINAPI _export Configure(int ItemNumber)
{
	return FALSE;
}
