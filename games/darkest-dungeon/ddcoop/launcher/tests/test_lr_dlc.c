/* Read-only DLC data gate exercised using disposable files, never real saves. */
#include "lr_dlc.h"
#include "lr_files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
static int failures;
#define CHECK(x) do { if(!(x)){printf("FAIL DLC:%d %s\n",__LINE__,#x);failures++;} }while(0)
typedef struct {const char *name,*package,*files[3],*base;} sample;
/* Independent fixture paths form the current campaign-DLC layout contract. */
static const sample samples[]={
 {"musketeer","dlc/445700_musketeer",{"heroes/musketeer/musketeer.info.darkest","upgrades/heroes/musketeer.upgrades.json",NULL},"raid/camping/default.camping_skills.json"},
 {"flagellant","dlc/580100_crimson_court/features/flagellant",{"heroes/flagellant/flagellant.info.darkest","upgrades/heroes/flagellant.upgrades.json","raid/camping/crimson_court.camping_skills.json"},NULL},
 {"districts","dlc/580100_crimson_court/features/districts",{"campaign/town/districts/districts_districts.json",NULL,NULL},NULL},
 {"crimson_court","dlc/580100_crimson_court/features/crimson_court",{"campaign/quest/crimson_court.quest.plot_quests.json","campaign/roster/roster.contagion.json",NULL},NULL},
 {"shieldbreaker","dlc/702540_shieldbreaker",{"heroes/shieldbreaker/shieldbreaker.info.darkest","upgrades/heroes/shieldbreaker.upgrades.json","raid/camping/shieldbreaker.camping_skills.json"},NULL},
 {"color_of_madness","dlc/735730_color_of_madness",{"campaign/quest/com.quest.plot_quests.json","campaign/provision/com.provision.json",NULL},NULL},
 {"fires_edge","dlc/4964110_fires_edge/features/fires_edge",{"campaign/town/districts/runaway_duelist.districts.json",NULL,NULL},NULL},
 {"runaway","dlc/4964110_fires_edge/features/runaway",{"heroes/runaway/runaway.info.darkest","upgrades/heroes/runaway.upgrades.json","raid/camping/runaway.camping_skills.json"},NULL},
 {"duelist","dlc/4964110_fires_edge/features/duelist",{"heroes/duelist/duelist.info.darkest","upgrades/heroes/duelist.upgrades.json","raid/camping/duelist.camping_skills.json"},NULL},
};
static void put(const char *root,const char *relative,size_t len) {
 char path[LR_PATH_MAX],parent[LR_PATH_MAX];lr_join(path,sizeof path,root,relative);strcpy(parent,path);
 char *a=strrchr(parent,'/'),*b=strrchr(parent,'\\'),*last=a && (!b || a>b)?a:b;
 CHECK(last!=NULL);if(!last)return;*last=0;CHECK(lr_make_dirs(parent));CHECK(lr_write_file(path,(const uint8_t *)"data",len));
}
int main(int argc,char **argv) {
 char root[LR_PATH_MAX];
#ifdef _WIN32
 snprintf(root,sizeof root,"build/dlc-test-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());
#else
 snprintf(root,sizeof root,"build/dlc-test-XXXXXX");CHECK(mkdtemp(root)!=NULL);
#endif
 CHECK(!lr_dlc_present(NULL,"musketeer") && !lr_dlc_present(root,NULL));
 CHECK(!lr_dlc_known("arena_mp") && !lr_dlc_known("future_feature") && !lr_dlc_present(root,"../musketeer"));
 for(size_t i=0;i<sizeof samples/sizeof *samples;i++) {
  const sample *s=&samples[i];char folder[LR_PATH_MAX];lr_join(folder,sizeof folder,root,s->package);CHECK(lr_make_dirs(folder));
  CHECK(lr_dlc_known(s->name) && !lr_dlc_present(root,s->name)); /* Empty package/feature must refuse. */
  size_t count=0;while(count<3 && s->files[count])count++;
  for(size_t j=0;j<count;j++) {CHECK(!lr_dlc_present(root,s->name));put(folder,s->files[j],4);}
  if(s->base) {CHECK(!lr_dlc_present(root,s->name));put(root,s->base,4);}
  CHECK(lr_dlc_present(root,s->name));
  put(folder,s->files[count-1],0);CHECK(!lr_dlc_present(root,s->name)); /* Empty critical data is incomplete. */
  put(folder,s->files[count-1],4);CHECK(lr_dlc_present(root,s->name));
 }
 if(argc==2)for(size_t i=0;i<sizeof samples/sizeof *samples;i++) {
  int ready=lr_dlc_present(argv[1],samples[i].name);printf("Installed read-only data %s: %s\n",samples[i].name,ready?"ready":"missing/incomplete");CHECK(ready);
 }
 printf("DLC data preflight: nine features, empty/partial/zero-length witnesses, base camping fallback, unknown/Circus refusal: %s (%d failures)\n",failures?"FAIL":"PASS",failures);
 return failures!=0;
}
