/* Solar-system guest program emitter. Included here to reuse the CPU assembler
 * and screen-space HUD/polygon emitters; runtime simulation stays in the guest. */
#include <math.h>
#define S_DT 10400u
#define S_LAST_TICK 10408u
#define S_POS 8600u
#define S_DIR 8704u
#define S_DIST 8728u
#define S_STEP 8736u
#define S_MANUAL 8484u
#define S_SYSTEM_SCALE 1000u
#define S_SYSTEM_DIVISOR 3u
#define S_SYSTEM_KM_S (9000u*S_SYSTEM_SCALE/S_SYSTEM_DIVISOR)
#define S_GROUP 8488u
#define S_CRUISE 3u
#define S_MOON_VIS 11900u
#define S_SYSTEM_RADIUS 12000u
#define S_NEAR_RADIUS 12104u
#define S_SCAN_FOUND 12400u
#define S_SCAN_YAW 12404u
#define S_SCAN_PITCH 12408u
#define S_HEADING 12412u
#define S_PITCH 12416u
#define S_IN_BELT 12420u
#define S_SCAN_DIST 12424u
#define S_SCAN_DIR 12432u
#define S_SUN_DIST 12456u
#define S_BELT_DIR 12464u
#define S_BELT_YAW 12488u
#define S_SCAN_STATE 12500u /* 0=idle, 1=locked, 2=no contact, 3=out of range */
#define S_SCAN_POS 12504u
#define S_SCAN_REQUEST 12528u
#define S_GUIDE 12700u /* 0=manual, 1=hold rock bearing, 2=fly to rock */
#define S_PROXIMITY 12704u
#define S_ROCK_CLEAR 12712u
#define S_SCAN_RADIUS 12728u
#define S_GUIDE_SPEED 12736u
#define S_ROCK_NEAR 12744u
#define S_WARN_DIR 12752u
#define S_WARN_REACH 12776u
#define S_GUIDE_REMAIN 12784u
#define S_INPUT_BLOCK 12808u /* Ignore pre-cruise held controls until released. */
#define S_MODE 8472u
#define S_EVENT 8476u
#define S_BODY 20000u
#define S_NEAREST 8480u
#define S_CLEAR 8752u
#define S_TARGET 8760u
#define S_CONST 18000u
#define S_CATALOG 25u
#define S_COUNT (S_CATALOG+1u) /* One scratch body for streamed geometry. */
#define S_PRIMARY 10u
#define S_COARSE (30000u+S_COUNT*4800u)
static uint8_t solar_data[S_COARSE+S_COUNT*1200u];
/* Catalogue order is stable: original planet IDs are retained. Distances in km.
 * Satellite distances are measured from the parent center. See docs/solar-system.md. */
static const char *solar_names[S_CATALOG]={"SOL","MERCURY","VENUS","EARTH","MARS","JUPITER","SATURN","URANUS","NEPTUNE","PLUTO",
  "MOON","PHOBOS","DEIMOS","IO","EUROPA","GANYMEDE","CALLISTO","TITAN","ENCELADUS","TITANIA","OBERON","TRITON","CHARON","CERES","VESTA"};
static double solar_radii[S_COUNT]={695700,2439.5,6052,6378,3396,71492,60268,25559,24764,1188.3,
  1737.4,11.1,6.2,1821.5,1560.8,2631.2,2410.3,2575,252,788.9,761.4,1353.4,606,473,265};
static double solar_orbits[S_COUNT]={0,57.9e6,108.2e6,149.6e6,228e6,778.5e6,1432e6,2867e6,4515e6,5906.4e6,
  384400,9378,23459,421800,671100,1070400,1882700,1221830,238020,436300,583500,354760,19596,2.768*149.6e6,2.36*149.6e6};
static const unsigned solar_parent[S_COUNT]={0,0,0,0,0,0,0,0,0,0,3,4,4,5,5,5,5,6,6,7,7,8,9,0,0};
static double solar_angles[S_COUNT]={0,.6,1.6,0,2.4,3.2,4.1,5,5.8,2.9,
  1.2,.8,3.1,.4,1.8,3.3,4.8,1.1,4.2,.7,3.8,2.1,1.5,1.9,4.5};
static double solar_centers[S_COUNT][3];
/* Groups share catalogue IDs; parent systems start with the parent itself. */
static const struct {
  const char *name;
  unsigned count, bodies[10];
} solar_groups[]={
  {"PLANETS",10,{0,1,2,3,4,5,6,7,8,9}},
  {"EARTH SYSTEM",2,{3,10}},
  {"MARS SYSTEM",3,{4,11,12}},
  {"JUPITER SYSTEM",5,{5,13,14,15,16}},
  {"SATURN SYSTEM",3,{6,17,18}},
  {"URANUS SYSTEM",3,{7,19,20}},
  {"NEPTUNE SYSTEM",2,{8,21}},
  {"PLUTO SYSTEM",2,{9,22}},
  {"ASTEROID BELT",2,{23,24}}
};
#define S_GROUPS (sizeof(solar_groups)/sizeof(solar_groups[0]))
/* Preserve original diagnostic addresses; avoid polygon scratch at 9000. */
static unsigned s_lod_address(unsigned b) { return b<S_CATALOG?8872+b*4:9600+(b-S_CATALOG)*4; }
static unsigned s_label_address(unsigned b) { return b<S_CATALOG?9300+b*4:10000+(b-S_CATALOG)*4; }
static void b_update(void);
static void b_sweep(void);
static void b_render(void);
static void b_nearest(void);
static void b_scan(void);
static void b_init_data(void);
static void s_instruments(void);
static void s_nearby(void);
static void s_data64(unsigned a,double v) {
  uint64_t bits;memcpy(&bits,&v,8);for(unsigned k=0;k<8;k++) solar_data[a+k]=(uint8_t)(bits>>(k*8));
}
static void s_data32(unsigned a,float v) {
  uint32_t bits;memcpy(&bits,&v,4);for(unsigned k=0;k<4;k++) solar_data[a+k]=(uint8_t)(bits>>(k*8));
}
static void sl(unsigned d,unsigned a) { e(MOV_RI(0,a));f(OP_FLOAD64,d,0); }
static void ss(unsigned d,unsigned a) { e(MOV_RI(0,a));f(OP_FSTORE64,d,0); }
static void si(unsigned a,unsigned v) { e(MOV_RI(0,a));e(MOV_RI(2,v));f(OP_STORE32,2,0); }
static void sr(unsigned d,unsigned a) { e(MOV_RI(0,a));f(OP_LOAD32,d,0); }
static void sroot(void) {
  copy(6,4);loadf(7,16400);f(OP_FADD_RR,6,7);loadf(8,8512);e(MOV_RI(2,64));size_t loop=pc;
  copy(7,4);f(OP_FDIV_RR,7,6);f(OP_FADD_RR,6,7);f(OP_FMUL_RR,6,8);
  e(SUB_RI(2,1));e(CMP_RI(2,0));e(JG(loop));copy(4,6);
}
static void s_delta(unsigned body) {
  loadf(4,ZERO);
  for(unsigned k=0;k<3;k++) {
    sl(5,S_BODY+body*64+k*8);sl(6,S_POS+k*8);f(OP_FSUB_RR,5,6);ss(5,8800+k*8);
    f(OP_FMUL_RR,5,5);f(OP_FADD_RR,4,5);
  }
}
static void s_target(void) {
  sr(8,8464);
  size_t exits[S_CATALOG];
  for(unsigned b=0;b<S_CATALOG;b++) {
    e(CMP_RI(8,b));size_t skip=pc++;
    s_delta(b);sroot();ss(4,S_DIST);
    sl(5,S_BODY+b*64+40);ss(5,S_TARGET);
    for(unsigned k=0;k<3;k++) { sl(5,8800+k*8);f(OP_FDIV_RR,5,4);ss(5,S_DIR+k*8); }
    exits[b]=pc++;code[skip]=JNE(pc);
  }
  for(unsigned b=0;b<S_CATALOG;b++) code[exits[b]]=JMP(pc);
}
/* Proximity gates apply even to selected moons. Thresholds enclose each system. */
static void s_context(void) {
  for(unsigned parent=1;parent<S_PRIMARY;parent++) {
    si(S_MOON_VIS+parent*4,0);s_delta(parent);
    sl(5,S_SYSTEM_RADIUS+parent*8);f(OP_FMUL_RR,5,5);f(OP_FCMP_RR,4,5);size_t distant=pc++;
    si(S_MOON_VIS+parent*4,1);code[distant]=JG(pc);
  }
}
static void s_arrival_group(void) {
  sr(8,8464);
  for(unsigned group=1;group<S_GROUPS-1;group++) {
    e(CMP_RI(8,solar_groups[group].bodies[0]));size_t other=pc++;
    si(S_GROUP,group);code[other]=JNE(pc);
  }
}
/* Choose a heading from trig tables; travel itself uses the continuous direction. */
static void s_bearing(unsigned direction,unsigned yaw,unsigned pitch) {
  for(unsigned axis=0;axis<2;axis++) {
    sl(9,direction);sl(10,direction+16);
    if(axis) {
      copy(4,9);f(OP_FMUL_RR,4,9);f(OP_FMUL_RR,10,10);f(OP_FADD_RR,4,10);sroot();copy(10,4);
      loadf(9,ZERO);sl(5,direction+8);f(OP_FSUB_RR,9,5);
    }
    loadf(11,8516);e(MOV_RI(6,0));e(MOV_RI(7,0));size_t loop=pc;
    trig(6,4,5);f(OP_FMUL_RR,4,9);f(OP_FMUL_RR,5,10);f(OP_FADD_RR,4,5);
    f(OP_FCMP_RR,4,11);size_t worse=pc++;
    copy(11,4);f(OP_MOV_RR,7,6);code[worse]=ENC_I24(OP_JLE,pc);
    e(ADD_RI(6,1));e(CMP_RI(6,256));e(JL(loop));f(OP_MOV_RR,axis?pitch:yaw,7);
  }
}
static void s_aim(void) { s_bearing(S_DIR,9,10); }
static void s_cancel_guide(void) {
  sr(2,S_GUIDE);e(CMP_RI(2,2));size_t manual=pc++;
  e(MOV_RI(11,0));code[manual]=JNE(pc);si(S_GUIDE,0);loadf(4,ZERO);ss(4,S_GUIDE_SPEED);
}
static void s_rock_arrived(void) {
  si(S_GUIDE,0);si(S_MANUAL,2);si(S_EVENT,9);e(MOV_RI(11,0));loadf(5,ZERO);ss(5,S_GUIDE_SPEED);
}
/* Emit guidance before the normal movement paths. The returned jump joins the
 * common swept-collision path, so assisted travel cannot bypass any obstacle. */
static size_t s_guidance(void) {
  sr(8,S_GUIDE);e(CMP_RI(8,0));size_t inactive=pc++;
  sr(8,S_SCAN_FOUND);e(CMP_RI(8,0));size_t lost=pc++;
  loadf(4,ZERO);
  for(unsigned k=0;k<3;k++) {
    sl(5,S_SCAN_POS+k*8);sl(6,S_POS+k*8);f(OP_FSUB_RR,5,6);ss(5,S_SCAN_DIR+k*8);
    f(OP_FMUL_RR,5,5);f(OP_FADD_RR,4,5);
  }
  sroot();ss(4,S_SCAN_DIST);
  for(unsigned k=0;k<3;k++){sl(5,S_SCAN_DIR+k*8);f(OP_FDIV_RR,5,4);ss(5,S_DIR+k*8);}
  s_aim();sr(8,S_GUIDE);e(CMP_RI(8,1));size_t aligned=pc++;
  sl(4,S_SCAN_DIST);sl(5,S_SCAN_RADIUS);sl(6,S_CONST+128);f(OP_FADD_RR,5,6);f(OP_FSUB_RR,4,5);ss(4,S_GUIDE_REMAIN);
  loadf(5,ZERO);f(OP_FCMP_RR,4,5);size_t already=pc++;
  /* Rock cruise holds SYSTEM speed. Clip the final step and stop automatically,
   * just as planetary cruise does, including after a long elapsed update. */
  sl(4,S_CONST+112);
  ss(4,S_GUIDE_SPEED);e(MOV_RI(2,10000));f(OP_ITOF,5,2);copy(6,4);f(OP_FDIV_RR,6,5);f(OP_FTOI,11,6);
  sl(5,S_DT);f(OP_FMUL_RR,4,5);e(MOV_RI(2,60));f(OP_ITOF,5,2);f(OP_FDIV_RR,4,5);
  sl(5,S_GUIDE_REMAIN);f(OP_FCMP_RR,4,5);size_t moving=pc++;
  copy(4,5);s_rock_arrived();code[moving]=JL(pc);size_t step_ready=pc++;
  code[already]=ENC_I24(OP_JLE,pc);s_rock_arrived();loadf(4,ZERO);
  code[step_ready]=JMP(pc);ss(4,S_STEP);size_t swept=pc++;
  code[lost]=ENC_I24(OP_JE,pc);s_cancel_guide();si(S_EVENT,8);
  code[inactive]=ENC_I24(OP_JE,pc);code[aligned]=ENC_I24(OP_JE,pc);return swept;
}
/* Swept ray/sphere entry: test the entire proposed step against every body. */
static void s_sweep(void) {
  for(unsigned b=0;b<S_CATALOG;b++) {
    s_delta(b);
    loadf(9,ZERO);
    for(unsigned k=0;k<3;k++) { sl(5,8800+k*8);sl(6,S_DIR+k*8);f(OP_FMUL_RR,5,6);f(OP_FADD_RR,9,5); }
    loadf(5,ZERO);f(OP_FCMP_RR,9,5);size_t behind=pc++;
    ss(9,8832);loadf(10,ZERO);
    for(unsigned k=0;k<3;k++) {
      sl(5,S_DIR+k*8);f(OP_FMUL_RR,5,9);sl(6,8800+k*8);f(OP_FSUB_RR,6,5);
      f(OP_FMUL_RR,6,6);f(OP_FADD_RR,10,6);
    }
    sl(4,S_BODY+b*64+32);f(OP_FMUL_RR,4,4);f(OP_FSUB_RR,4,10);
    loadf(5,ZERO);f(OP_FCMP_RR,4,5);size_t miss=pc++;
    sroot();sl(9,8832);f(OP_FSUB_RR,9,4);sl(5,S_CONST+8);f(OP_FSUB_RR,9,5); /* stop 1 km before boundary */
    loadf(5,ZERO);f(OP_FCMP_RR,9,5);size_t positive=pc++;copy(9,5);code[positive]=ENC_I24(OP_JGE,pc);
    sl(5,S_STEP);f(OP_FCMP_RR,9,5);size_t farther=pc++;
    ss(9,S_STEP);si(S_MODE,0);s_cancel_guide();si(S_EVENT,2);e(MOV_RI(11,0));
    code[behind]=ENC_I24(OP_JLE,pc);code[miss]=JL(pc);code[farther]=ENC_I24(OP_JGE,pc);
  }
}
static void s_move(void) {
  size_t guided=s_guidance();
  sr(8,S_MODE);e(CMP_RI(8,0));size_t local=pc++;
  s_target();sl(4,S_DIST);sl(5,S_TARGET);f(OP_FSUB_RR,4,5);
  loadf(5,ZERO);f(OP_FCMP_RR,4,5);size_t away=pc++;
  si(S_MODE,0);si(S_EVENT,1);e(MOV_RI(11,0));s_arrival_group();loadf(4,ZERO);size_t near=pc++;
  code[away]=JG(pc);
  /* Constant cruise step; brake immediately at the arrival shell. */
  sl(5,S_CONST);sl(6,S_DT);f(OP_FMUL_RR,5,6);
  f(OP_FCMP_RR,4,5);size_t short_step=pc++;
  copy(4,5);size_t capped=pc++;
  code[short_step]=ENC_I24(OP_JLE,pc);si(S_MODE,0);si(S_EVENT,1);e(MOV_RI(11,0));s_arrival_group();
  code[capped]=JMP(pc);code[near]=JMP(pc);ss(4,S_STEP);s_aim();size_t sweep=pc++;
  code[local]=ENC_I24(OP_JE,pc);
  trig(9,0,1);trig(10,2,3);
  copy(5,0);f(OP_FMUL_RR,5,3);ss(5,S_DIR);
  loadf(5,ZERO);f(OP_FSUB_RR,5,2);ss(5,S_DIR+8);
  copy(5,1);f(OP_FMUL_RR,5,3);ss(5,S_DIR+16);
  sr(8,S_MANUAL);sl(5,S_CONST+16);e(CMP_RI(8,1));size_t system=pc++;
  e(MOV_RI(2,S_SYSTEM_SCALE));f(OP_ITOF,6,2);f(OP_FMUL_RR,5,6);
  e(MOV_RI(2,S_SYSTEM_DIVISOR));f(OP_ITOF,6,2);f(OP_FDIV_RR,5,6);code[system]=JNE(pc);
  e(CMP_RI(8,2));size_t precision=pc++;sl(5,S_CONST+88);code[precision]=JNE(pc);
  f(OP_ITOF,4,11);f(OP_FMUL_RR,4,5);sl(5,S_DT);f(OP_FMUL_RR,4,5);ss(4,S_STEP);
  sr(8,S_MANUAL);e(CMP_RI(8,S_CRUISE));size_t manual=pc++;
  loadf(4,ZERO);ss(4,S_STEP);code[manual]=JNE(pc);
  code[sweep]=JMP(pc);code[guided]=JMP(pc);s_sweep();b_sweep();
  for(unsigned k=0;k<3;k++) { sl(5,S_DIR+k*8);sl(6,S_STEP);f(OP_FMUL_RR,5,6);sl(6,S_POS+k*8);f(OP_FADD_RR,5,6);ss(5,S_POS+k*8); }
}
static void s_ack_manual_press(unsigned k) {
  e(ENC_RI(OP_LUI,0,INPUT_BASE>>12));e(ADD_RI(0,INPUT_ACK+k*4));e(MOV_RI(2,1));f(OP_STORE32,2,0);
}
static void s_take_controls(void) {
  for(unsigned k=0;k<6;k++) {
    key(k,2);e(MOV_RI(0,S_INPUT_BLOCK+k*4));f(OP_STORE32,2,0);s_ack_manual_press(k);
  }
}
/* A control already held when cruise starts cannot cancel it or resume motion
 * after arrival. Releasing it restores normal input; a fresh press can override. */
static void s_pilot_key(unsigned k,unsigned dst) {
  key(k,dst);e(CMP_RI(dst,0));size_t down=pc++;
  e(MOV_RI(0,S_INPUT_BLOCK+k*4));e(MOV_RI(1,0));f(OP_STORE32,1,0);
  code[down]=JNE(pc);sr(1,S_INPUT_BLOCK+k*4);e(CMP_RI(1,0));size_t unblocked=pc++;
  e(ENC_RI(OP_LUI,0,INPUT_BASE>>12));e(ADD_RI(0,INPUT_PRESSED+k*4));f(OP_LOAD32,1,0);e(CMP_RI(1,0));size_t blocked=pc++;
  e(MOV_RI(0,S_INPUT_BLOCK+k*4));e(MOV_RI(1,0));f(OP_STORE32,1,0);size_t fresh=pc++;
  code[blocked]=ENC_I24(OP_JE,pc);e(MOV_RI(dst,0));code[fresh]=JMP(pc);code[unblocked]=ENC_I24(OP_JE,pc);
}
static void s_pilot_control(unsigned reg,unsigned negative,unsigned positive,unsigned low,unsigned high,bool wrap) {
  s_pilot_key(negative,2);s_pilot_key(positive,3);e(CMP_RR(2,3));size_t idle=pc++;e(CMP_RI(2,0));size_t plus=pc++;
  e(CMP_RI(reg,low));size_t bottom=pc++;e(SUB_RI(reg,1));size_t minus_done=pc++;
  code[bottom]=ENC_I24(OP_JLE,pc);e(MOV_RI(reg,wrap?high:low));size_t bottom_done=pc++;
  code[plus]=ENC_I24(OP_JE,pc);e(CMP_RI(reg,high));size_t top=pc++;e(ADD_RI(reg,1));size_t plus_done=pc++;
  code[top]=ENC_I24(OP_JGE,pc);e(MOV_RI(reg,wrap?low:high));
  code[idle]=ENC_I24(OP_JE,pc);code[minus_done]=JMP(pc);code[bottom_done]=JMP(pc);code[plus_done]=JMP(pc);
}
static void s_press(unsigned key,unsigned kind) {
  unsigned bank=key>=8?0x100:0,slot=key%8;
  e(ENC_RI(OP_LUI,0,INPUT_BASE>>12));e(ADD_RI(0,bank+INPUT_PRESSED+slot*4));f(OP_LOAD32,2,0);
  e(CMP_RI(2,0));size_t skip=pc++;
  if(kind==1||kind==3||kind==4||kind==5||kind==6)s_cancel_guide();
  if(kind==0) {sr(2,8460);e(MOV_RI(3,1));f(OP_SUB_RR,3,2);e(MOV_RI(0,8460));f(OP_STORE32,3,0);}
  else if(kind==1) {
    sr(8,S_GROUP);sr(7,8464);
    for(unsigned group=0;group<S_GROUPS;group++) {
      e(CMP_RI(8,group));size_t other_group=pc++;e(MOV_RI(3,solar_groups[group].bodies[0]));
      for(unsigned item=0;item<solar_groups[group].count;item++) {
        e(CMP_RI(7,solar_groups[group].bodies[item]));size_t other=pc++;
        e(MOV_RI(3,solar_groups[group].bodies[(item+1)%solar_groups[group].count]));code[other]=JNE(pc);
      }
      e(MOV_RI(0,8464));f(OP_STORE32,3,0);code[other_group]=JNE(pc);
    }
    si(S_MODE,0);si(S_EVENT,0);e(MOV_RI(11,0));
  } else if(kind==3) {
    sr(3,S_MANUAL);e(ADD_RI(3,1));e(CMP_RI(3,4));size_t wrap=pc++;e(MOV_RI(3,0));code[wrap]=JL(pc);e(MOV_RI(0,S_MANUAL));f(OP_STORE32,3,0);
    si(S_MODE,0);si(S_EVENT,0);e(MOV_RI(11,0));
  } else if(kind==4) {
    sr(3,S_GROUP);e(ADD_RI(3,1));e(CMP_RI(3,S_GROUPS));size_t wrap=pc++;e(MOV_RI(3,0));code[wrap]=JL(pc);
    e(MOV_RI(0,S_GROUP));f(OP_STORE32,3,0);
    for(unsigned group=0;group<S_GROUPS;group++) {
      e(CMP_RI(3,group));size_t other=pc++;si(8464,solar_groups[group].bodies[0]);code[other]=JNE(pc);
    }
    si(S_MODE,0);si(S_EVENT,0);e(MOV_RI(11,0));
  } else if(kind==5) {
    si(S_SCAN_REQUEST,1);
  } else if(kind==6) {
    si(S_SCAN_REQUEST,0);si(S_SCAN_FOUND,0);si(S_SCAN_STATE,0);
  } else if(kind==7||kind==8) {
    unsigned guide=kind==7?1:2;
    sr(8,S_GUIDE);e(CMP_RI(8,guide));size_t enable=pc++;s_cancel_guide();si(S_EVENT,0);size_t cancelled=pc++;
    code[enable]=JNE(pc);sr(8,S_MANUAL);e(CMP_RI(8,kind==8?1:S_CRUISE));size_t mode_ok=pc++;
    si(S_EVENT,kind==8?6:7);size_t wrong_mode=pc++;
    code[mode_ok]=kind==8?ENC_I24(OP_JE,pc):JNE(pc);
    sr(8,S_SCAN_FOUND);e(CMP_RI(8,0));size_t no_target=pc++;
    sr(8,S_SCAN_REQUEST);e(CMP_RI(8,0));size_t pending=pc++;
    s_cancel_guide();si(S_MODE,0);si(S_GUIDE,guide);si(S_EVENT,0);if(guide==2)s_take_controls();size_t started=pc++;
    code[no_target]=ENC_I24(OP_JE,pc);code[pending]=JNE(pc);si(S_EVENT,5);
    code[started]=JMP(pc);code[cancelled]=JMP(pc);code[wrong_mode]=JMP(pc);
  } else {
    sr(8,S_MANUAL);e(CMP_RI(8,S_CRUISE));size_t armed=pc++;si(S_EVENT,3);size_t disabled=pc++;
    code[armed]=ENC_I24(OP_JE,pc);
    sr(8,S_MODE);e(CMP_RI(8,0));size_t starting=pc++;
    si(S_MODE,0);si(S_EVENT,0);e(MOV_RI(11,0));size_t cancelled=pc++;
    code[starting]=ENC_I24(OP_JE,pc);s_target();
    sr(8,8464);e(MOV_RI(2,8));f(OP_MUL_RR,8,2);e(ADD_RI(8,S_NEAR_RADIUS));f(OP_FLOAD64,5,8);
    sl(4,S_DIST);f(OP_FCMP_RR,4,5);size_t far=pc++;
    si(S_EVENT,4);size_t nearby=pc++;
    code[far]=JG(pc);si(S_MODE,1);si(S_EVENT,0);e(MOV_RI(11,0));
    code[disabled]=JMP(pc);code[cancelled]=JMP(pc);code[nearby]=JMP(pc);
  }
  e(ENC_RI(OP_LUI,0,INPUT_BASE>>12));e(ADD_RI(0,bank+INPUT_ACK+slot*4));e(MOV_RI(2,1));f(OP_STORE32,2,0);
  code[skip]=ENC_I24(OP_JE,pc);
}
static void s_camera(unsigned body,unsigned vertex) {
  for(unsigned k=0;k<3;k++) {
    sl(11+k,S_BODY+body*64+k*8);sl(14,S_POS+k*8);f(OP_FSUB_RR,11+k,14);
    if(vertex) {
      loadf(14,vertex+k*4);
      if(body==S_CATALOG) {sl(15,S_BODY+body*64+24);f(OP_FMUL_RR,14,15);sl(15,S_BODY+body*64+40+k*8);f(OP_FMUL_RR,14,15);}
      f(OP_FADD_RR,11+k,14);
    }
  }
  copy(5,11);f(OP_FMUL_RR,5,1);copy(14,13);f(OP_FMUL_RR,14,0);f(OP_FSUB_RR,5,14);
  f(OP_FMUL_RR,11,0);f(OP_FMUL_RR,13,1);f(OP_FADD_RR,11,13);
  copy(6,11);f(OP_FMUL_RR,6,2);copy(14,12);f(OP_FMUL_RR,14,3);f(OP_FADD_RR,6,14);
  copy(7,11);f(OP_FMUL_RR,7,3);f(OP_FMUL_RR,12,2);f(OP_FSUB_RR,7,12);
}
static void s_face(unsigned body,unsigned face,bool coarse) {
  unsigned address=coarse?S_COARSE+body*1200+face*60:30000+body*4800+face*60;
  for(unsigned v=0;v<3;v++) {s_camera(body,address+v*12);for(unsigned k=0;k<3;k++) storef(5+k,9000+v*12+k*4);}
  unsigned colors[S_COUNT]={0xffb32b,0x9b9488,0xd8b16b,0x337fc7,0xb96a48,0xc99d72,0xd9c18a,0x7fcbd1,0x426bd3,0xc5aaa0,
    0xbcbcbc,0x9e8873,0xa28f80,0xd2bd52,0xd8c6a5,0x9e9180,0x82766c,0xd8ad65,0xe2efff,0xa8a6ac,0x929095,0xb7c5ce,0xa8a2a0,0x918d88,0xa69888};
  unsigned color=body<S_CATALOG?colors[body]:0x514c47;
  if(body==3 && solid_faces[face][2]>solid_faces[face][3]) color=0x399b60;
  if(body==0) loadf(4,16400);
  else {
    loadf(4,address+36);if(body==S_CATALOG)sl(5,10720);else loadf(5,address+48);f(OP_FMUL_RR,4,5);
    for(unsigned k=1;k<3;k++) {loadf(5,address+36+k*4);if(body==S_CATALOG)sl(6,10720+k*8);else loadf(6,address+48+k*4);f(OP_FMUL_RR,5,6);f(OP_FADD_RR,4,5);}
    loadf(5,ZERO);f(OP_FCMP_RR,4,5);size_t positive=pc++;copy(4,5);code[positive]=ENC_I24(OP_JGE,pc);
    loadf(5,16392);f(OP_FMUL_RR,4,5);loadf(5,16388);f(OP_FADD_RR,4,5);
  }
  e(MOV_RI(3,0));
  for(unsigned k=0;k<3;k++) {
    e(MOV_RI(2,(color>>(16-8*k))&255));f(OP_ITOF,5,2);f(OP_FMUL_RR,5,4);f(OP_FTOI,2,5);
    e(MOV_RI(1,k==0?65536:k==1?256:1));f(OP_MUL_RR,2,1);f(OP_ADD_RR,3,2);
  }
  gpu_reg(GPU_COLOR,3);polygon();
}
/* Conservative bounding-sphere view test before any vertex projection.
 * The side-plane normals are not unit length, so scale radius accordingly. */
static void s_frustum(unsigned body,size_t rejected[5]) {
  s_camera(body,0);sl(10,S_BODY+body*64+24);
  copy(11,7);f(OP_FADD_RR,11,10);loadf(15,NEAR);f(OP_FCMP_RR,11,15);rejected[0]=pc++;
  for(unsigned plane=0;plane<4;plane++) {
    unsigned axis=plane<2?5:6;
    loadf(15,plane<2?CX:CY);loadf(14,FOCAL);f(OP_FDIV_RR,15,14);
    copy(11,7);f(OP_FMUL_RR,11,15);f(plane%2?OP_FSUB_RR:OP_FADD_RR,11,axis);
    loadf(15,plane<2?8520:8524);copy(12,10);f(OP_FMUL_RR,12,15);
    f(OP_FADD_RR,11,12);loadf(15,ZERO);f(OP_FCMP_RR,11,15);rejected[plane+1]=pc++;
  }
}
static void s_point(unsigned body) {
  s_camera(body,0);loadf(15,NEAR);f(OP_FCMP_RR,7,15);size_t behind=pc++;
  loadf(15,FOCAL);f(OP_FMUL_RR,5,15);f(OP_FDIV_RR,5,7);f(OP_FMUL_RR,6,15);f(OP_FDIV_RR,6,7);
  loadf(15,CX);f(OP_FADD_RR,5,15);loadf(15,CY);f(OP_FSUB_RR,15,6);copy(6,15);
  size_t skip[4];loadf(15,ZERO);f(OP_FCMP_RR,5,15);skip[0]=pc++;f(OP_FCMP_RR,6,15);skip[1]=pc++;
  e(MOV_RI(2,1023));f(OP_ITOF,15,2);f(OP_FCMP_RR,5,15);skip[2]=pc++;
  e(MOV_RI(2,599));f(OP_ITOF,15,2);f(OP_FCMP_RR,6,15);skip[3]=pc++;
  f(OP_FTOI,5,5);f(OP_FTOI,6,6);gpu_reg(GPU_X,5);gpu_reg(GPU_Y,6);
  pc=emit_gpu_write(code,pc,GPU_COLOR,body?0x9bd7ff:0xffd060);pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_PIXEL);
  code[behind]=ENC_I24(OP_JLE,pc);code[skip[0]]=JL(pc);code[skip[1]]=JL(pc);code[skip[2]]=JG(pc);code[skip[3]]=JG(pc);
}
static void s_name(unsigned id,unsigned x,unsigned y) {
  for(unsigned b=0;b<S_CATALOG;b++) {e(CMP_RI(id,b));size_t skip=pc++;hud_text(solar_names[b],x,y);code[skip]=JNE(pc);}
  e(CMP_RI(id,S_CATALOG));size_t named=pc++;hud_text("ASTEROID",x,y);code[named]=JL(pc);
}
/* Screen annotation origin is R6/R7. Only R0/R1/R2 are scratch. */
static void s_label_line(int x,int y,int x2,int y2) {
  const int offsets[]={x,y,x2,y2};
  const unsigned regs[]={GPU_X,GPU_Y,GPU_X2,GPU_Y2};
  for(unsigned k=0;k<4;k++) {
    f(OP_MOV_RR,2,k%2?7:6);
    if(offsets[k]>=0) e(ADD_RI(2,(unsigned)offsets[k]));
    else e(SUB_RI(2,(unsigned)-offsets[k]));
    gpu_reg(regs[k],2);
  }
  pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_LINE);
}
/* Solid 5x7 lettering at 2x scale: no gaps inside a glyph stroke. */
static void s_label_text(const char *name,int x,int y) {
  static const unsigned char letters[][7]={
    {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
    {7,2,2,2,2,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
    {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14}};
  for(;*name;name++,x+=12) {
    unsigned g=*name>='A'&&*name<='Z'?(unsigned)(*name-'A'):*name>='0'&&*name<='9'?(unsigned)(*name-'0'+26):36;
    if(g==36)continue;
    for(unsigned row=0;row<7;row++)for(unsigned col=0;col<5;col++)if(letters[g][row]&(16u>>col))
      for(unsigned dy=0;dy<2;dy++)s_label_line(x+(int)col*2,y+(int)row*2+dy,x+(int)col*2+1,y+(int)row*2+dy);
  }
}
/* Label boxes: 11000+16*body = x,y,width,height. Centers: 11500+8*body.
 * Selected destination is placed first. Other names move down or are omitted. */
static size_t s_label_place(unsigned body) {
  e(MOV_RI(0,11500+body*8));f(OP_STORE32,6,0);e(ADD_RI(0,4));f(OP_STORE32,7,0);
  e(CMP_RI(6,880));size_t left=pc++;e(ADD_RI(6,20));size_t side_done=pc++;
  code[left]=JG(pc);e(SUB_RI(6,140));code[side_done]=JMP(pc);e(SUB_RI(7,24));
  size_t retry=pc;e(CMP_RI(7,560));size_t full=pc++;
  /* Keep annotations clear of the permanent navigation instruments. */
  e(CMP_RI(6,474));size_t instruments_right=pc++;
  e(CMP_RI(7,98));size_t instruments_below=pc++;e(MOV_RI(7,98));
  code[instruments_right]=ENC_I24(OP_JGE,pc);code[instruments_below]=ENC_I24(OP_JGE,pc);
  sr(2,S_SCAN_STATE);e(CMP_RI(2,0));size_t no_scan=pc++;
  e(CMP_RI(6,418));size_t scan_right=pc++;
  e(CMP_RI(7,172));size_t scan_below=pc++;e(MOV_RI(7,172));
  code[no_scan]=ENC_I24(OP_JE,pc);code[scan_right]=ENC_I24(OP_JGE,pc);code[scan_below]=ENC_I24(OP_JGE,pc);
  sr(2,8460);e(CMP_RI(2,0));size_t map_hidden=pc++;
  f(OP_MOV_RR,2,6);e(ADD_RI(2,120));e(CMP_RI(2,616));size_t map_left=pc++;
  e(CMP_RI(7,394));size_t map_below=pc++;e(MOV_RI(7,394));
  code[map_hidden]=ENC_I24(OP_JE,pc);code[map_left]=ENC_I24(OP_JLE,pc);code[map_below]=ENC_I24(OP_JGE,pc);
  e(MOV_RI(5,0));size_t check=pc;
  f(OP_MOV_RR,0,5);e(MOV_RI(2,4));f(OP_MUL_RR,0,2);e(ADD_RI(0,9300));f(OP_LOAD32,2,0);e(CMP_RI(2,0));size_t unused=pc++;
  f(OP_MOV_RR,0,5);e(MOV_RI(2,16));f(OP_MUL_RR,0,2);e(ADD_RI(0,11000));f(OP_LOAD32,8,0);e(ADD_RI(0,4));f(OP_LOAD32,12,0);
  size_t clear[4];
  f(OP_MOV_RR,13,8);e(ADD_RI(13,124));e(CMP_RR(6,13));clear[0]=pc++;
  f(OP_MOV_RR,13,6);e(ADD_RI(13,124));e(CMP_RR(8,13));clear[1]=pc++;
  f(OP_MOV_RR,13,12);e(ADD_RI(13,38));e(CMP_RR(7,13));clear[2]=pc++;
  f(OP_MOV_RR,13,7);e(ADD_RI(13,38));e(CMP_RR(12,13));clear[3]=pc++;
  e(ADD_RI(7,38));e(JMP(retry));
  code[unused]=ENC_I24(OP_JE,pc);for(unsigned k=0;k<4;k++)code[clear[k]]=ENC_I24(OP_JGE,pc);
  e(ADD_RI(5,1));e(CMP_RI(5,S_CATALOG));e(JL(check));
  e(MOV_RI(0,11000+body*16));f(OP_STORE32,6,0);e(ADD_RI(0,4));f(OP_STORE32,7,0);
  si(11000+body*16+8,120);si(11000+body*16+12,34);si(s_label_address(body),1);
  return full;
}
static void s_label_range(int x,int y) {
  size_t exits[4];
  for(unsigned unit=0;unit<4;unit++) {
    sl(5,S_CONST+24+unit*8);copy(9,4);f(OP_FDIV_RR,9,5);
    e(MOV_RI(2,999));f(OP_ITOF,5,2);f(OP_FCMP_RR,9,5);size_t next=pc++;
    f(OP_FTOI,8,9);
    s_label_text(unit==0?"KM":unit==1?"KKM":unit==2?"MKM":"AU",x+48,y);
    exits[unit]=pc++;code[next]=JG(pc);
  }
  s_label_text("FAR",x,y);size_t far=pc++;
  for(unsigned unit=0;unit<4;unit++)code[exits[unit]]=JMP(pc);
  for(unsigned digit=0,divisor=100;digit<3;digit++,divisor/=10) {
    f(OP_MOV_RR,12,8);e(MOV_RI(13,divisor));f(OP_DIV_RR,12,13);
    for(unsigned n=0;n<10;n++) {
      e(CMP_RI(12,n));size_t other=pc++;char text[2]={(char)('0'+n),0};
      s_label_text(text,x+(int)digit*12,y);code[other]=JNE(pc);
    }
    f(OP_MUL_RR,12,13);f(OP_SUB_RR,8,12);
  }
  code[far]=JMP(pc);
}
static void s_label_depth(unsigned body) {
  s_camera(body,0);loadf(15,16384);f(OP_FDIV_RR,15,7);f(OP_FTOI,2,15);
  e(CMP_RI(2,1));size_t nonzero=pc++;e(MOV_RI(2,1));code[nonzero]=ENC_I24(OP_JGE,pc);
  gpu_reg(GPU_Q1,2);gpu_reg(GPU_Q2,2);
}
static void s_labels(void) {
  pc=emit_gpu_write(code,pc,GPU_DEPTH,GPU_DEPTH_TEST_ONLY);
  for(unsigned b=0;b<S_CATALOG;b++){si(s_label_address(b),0);si(11600+b*4,0);}
  sr(2,8464);e(MOV_RI(0,10816));f(OP_STORE32,2,0);
  si(10812,0);size_t pass=pc;
  for(unsigned b=0;b<S_CATALOG;b++) {
    sr(2,10816);e(CMP_RI(2,b));size_t not_chosen=pc++;si(11600+b*4,1);
    size_t remote_moon=0;
    if(solar_parent[b]) {sr(2,S_MOON_VIS+solar_parent[b]*4);e(CMP_RI(2,0));remote_moon=pc++;}
    s_camera(b,0);
    loadf(15,NEAR);f(OP_FCMP_RR,7,15);size_t behind=pc++;
    /* Selected destinations remain identified. Other forward bodies receive
     * eight seconds of cruise lookahead, even below the angular threshold. */
    sr(8,8464);e(CMP_RI(8,b));size_t selected=pc++;
    sl(15,S_CONST+80);f(OP_FCMP_RR,7,15);size_t distant=pc++;
    copy(14,7);loadf(15,8512);f(OP_FMUL_RR,14,15);
    size_t side[4];
    for(unsigned axis=0;axis<2;axis++) {
      f(OP_FCMP_RR,5+axis,14);side[axis*2]=pc++;
      loadf(15,ZERO);f(OP_FSUB_RR,15,14);f(OP_FCMP_RR,5+axis,15);side[axis*2+1]=pc++;
    }
    size_t ahead=pc++;
    code[distant]=JG(pc);
    for(unsigned axis=0;axis<2;axis++) {code[side[axis*2]]=JG(pc);code[side[axis*2+1]]=JL(pc);}
    /* Ten times the former angular label range. */
    sl(15,S_BODY+b*64+24);e(MOV_RI(2,16000));f(OP_ITOF,14,2);f(OP_FMUL_RR,15,14);
    f(OP_FCMP_RR,7,15);size_t tiny=pc++;
    code[selected]=ENC_I24(OP_JE,pc);code[ahead]=JMP(pc);
    loadf(15,FOCAL);f(OP_FMUL_RR,5,15);f(OP_FDIV_RR,5,7);f(OP_FMUL_RR,6,15);f(OP_FDIV_RR,6,7);
    loadf(15,CX);f(OP_FADD_RR,5,15);loadf(15,CY);f(OP_FSUB_RR,15,6);copy(6,15);
    /* Bound floating coordinates before conversion; reserve label margins. */
    size_t outside[4];const unsigned bounds[]={12,1012,32,570};
    for(unsigned k=0;k<4;k++) {
      e(MOV_RI(2,bounds[k]));f(OP_ITOF,15,2);f(OP_FCMP_RR,k<2?5:6,15);outside[k]=pc++;
    }
    f(OP_FTOI,6,5);f(OP_FTOI,7,6);size_t full=s_label_place(b);
    code[full]=JG(pc);code[not_chosen]=JNE(pc);
    if(remote_moon)code[remote_moon]=ENC_I24(OP_JE,pc);
    code[behind]=ENC_I24(OP_JLE,pc);code[tiny]=JG(pc);
    for(unsigned k=0;k<4;k++)code[outside[k]]=k%2?JG(pc):JL(pc);
  }
  sr(2,10812);e(ADD_RI(2,1));e(MOV_RI(0,10812));f(OP_STORE32,2,0);e(CMP_RI(2,S_CATALOG));size_t complete=pc++;
  /* Prioritize large apparent bodies after the selected destination. */
  loadf(8,8516);si(10816,S_CATALOG);
  for(unsigned b=0;b<S_CATALOG;b++) {
    sr(2,11600+b*4);e(CMP_RI(2,0));size_t used=pc++;
    s_camera(b,0);loadf(15,NEAR);f(OP_FCMP_RR,7,15);size_t behind=pc++;
    sl(15,S_BODY+b*64+24);f(OP_FDIV_RR,15,7);f(OP_FCMP_RR,15,8);size_t smaller=pc++;
    copy(8,15);si(10816,b);code[used]=JNE(pc);code[behind]=ENC_I24(OP_JLE,pc);code[smaller]=ENC_I24(OP_JLE,pc);
  }
  e(JMP(pass));code[complete]=ENC_I24(OP_JGE,pc);
  /* All leaders first; opaque text panels then cover crossing lines. */
  for(unsigned b=0;b<S_CATALOG;b++) {
    sr(2,s_label_address(b));e(CMP_RI(2,0));size_t hidden=pc++;
    s_label_depth(b);
    sr(6,11500+b*8);sr(7,11504+b*8);gpu_reg(GPU_X,6);gpu_reg(GPU_Y,7);
    sr(6,11000+b*16);sr(7,11004+b*16);gpu_reg(GPU_X2,6);gpu_reg(GPU_Y2,7);
    pc=emit_gpu_write(code,pc,GPU_COLOR,0x466578);pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_LINE);code[hidden]=ENC_I24(OP_JE,pc);
  }
  for(unsigned b=0;b<S_CATALOG;b++) {
    sr(2,s_label_address(b));e(CMP_RI(2,0));size_t hidden=pc++;
    s_label_depth(b);
    sr(6,11000+b*16);sr(7,11004+b*16);
    pc=emit_gpu_write(code,pc,GPU_COLOR,0x03080c);
    for(int y=0;y<34;y++)s_label_line(0,y,119,y);
    pc=emit_gpu_write(code,pc,GPU_COLOR,0xb8dfff);
    sr(8,8464);e(CMP_RI(8,b));size_t normal=pc++;pc=emit_gpu_write(code,pc,GPU_COLOR,0x33ff44);code[normal]=JNE(pc);
    s_label_text(solar_names[b],3,1);s_delta(b);sroot();s_label_range(3,18);code[hidden]=ENC_I24(OP_JE,pc);
  }
}
static void s_range(unsigned x,unsigned y) {
  /* F4 km -> scaled 3-digit value with explicit unit label. */
  size_t exits[4];
  for(unsigned unit=0;unit<4;unit++) {
    sl(5,S_CONST+24+unit*8);copy(9,4);f(OP_FDIV_RR,9,5);
    e(MOV_RI(2,999));f(OP_ITOF,5,2);f(OP_FCMP_RR,9,5);size_t next=pc++;
    f(OP_FTOI,7,9);hud_number_unit(x,y,unit==0?"KM":unit==1?"KKM":unit==2?"MKM":"AU");exits[unit]=pc++;
    code[next]=JG(pc);
  }
  hud_text("FAR",x,y);for(unsigned u=0;u<4;u++) code[exits[u]]=JMP(pc);
}
/* SYSTEM has 300 increments of 10,000 km/s: show all of them as X.XX MKM/s. */
static void s_system_speed(void) {
  e(MOV_RI(2,10000));f(OP_ITOF,5,2);f(OP_FDIV_RR,4,5);f(OP_FTOI,7,4);
  for(unsigned place=0;place<3;place++) {
    unsigned divisor=place==0?100:place==1?10:1,x=24+place*12+(place?12:0);
    f(OP_MOV_RR,8,7);e(MOV_RI(2,divisor));f(OP_DIV_RR,8,2);
    f(OP_MOV_RR,3,8);f(OP_MUL_RR,3,2);f(OP_SUB_RR,7,3);
    for(unsigned digit=0;digit<10;digit++) {
      e(CMP_RI(8,digit));size_t other=pc++;
      char text[2]={(char)('0'+digit),0};hud_text(text,x,668);code[other]=JNE(pc);
    }
  }
  hud_line(39,680,41,680);hud_line(39,681,41,681);hud_text("MKM",84,668);
}
static void s_throttle(void) {
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x9bd7ff);hud_text("THROTTLE",374,610);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x305468);
  hud_line(482,610,663,610);hud_line(482,620,663,620);hud_line(482,610,482,620);hud_line(663,610,663,620);
  sr(8,S_GUIDE);e(CMP_RI(8,2));size_t guided=pc++;
  sr(8,S_MANUAL);e(CMP_RI(8,S_CRUISE));size_t automatic=pc++;
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x33ff44);
  f(OP_MOV_RR,7,11);e(MOV_RI(2,3));f(OP_DIV_RR,7,2);hud_number_unit(716,610,"PCT");
  f(OP_MOV_RR,6,11);e(MOV_RI(2,180));f(OP_MUL_RR,6,2);e(MOV_RI(2,300));f(OP_DIV_RR,6,2);
  e(CMP_RI(6,0));size_t zero=pc++;e(ADD_RI(6,482));gpu_reg(GPU_X2,6);
  pc=emit_gpu_write(code,pc,GPU_X,483);
  for(unsigned y=612;y<=618;y++) {
    pc=emit_gpu_write(code,pc,GPU_Y,y);pc=emit_gpu_write(code,pc,GPU_Y2,y);pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_LINE);
  }
  code[zero]=ENC_I24(OP_JE,pc);size_t manual_done=pc++;
  code[guided]=ENC_I24(OP_JE,pc);code[automatic]=ENC_I24(OP_JE,pc);
  sr(8,S_GUIDE);e(CMP_RI(8,2));size_t guiding=pc++;
  sr(8,S_MODE);e(CMP_RI(8,0));size_t ready=pc++;code[guiding]=ENC_I24(OP_JE,pc);
  hud_text("AUTO",716,610);size_t auto_done=pc++;
  code[ready]=ENC_I24(OP_JE,pc);hud_text("READY",716,610);
  code[auto_done]=JMP(pc);code[manual_done]=JMP(pc);
}
static void s_guide_speed_hud(void) {
  sl(4,S_GUIDE_SPEED);sl(5,S_CONST+8);f(OP_FCMP_RR,4,5);size_t metres=pc++;
  sl(5,S_CONST+40);f(OP_FCMP_RR,4,5);size_t smaller=pc++;s_system_speed();size_t large=pc++;
  code[smaller]=JL(pc);s_range(24,668);code[large]=JMP(pc);hud_text("S",144,668);size_t done=pc++;
  code[metres]=JL(pc);e(MOV_RI(2,1000));f(OP_ITOF,5,2);f(OP_FMUL_RR,4,5);f(OP_FTOI,7,4);hud_number_unit(24,668,"M S");code[done]=JMP(pc);
}
static void s_rock_warning(void) {
  const char *messages[]={"ASTEROID NEARBY","ROCK VERY CLOSE","BRAKE ROCK AHEAD"};
  sr(8,S_PROXIMITY);
  for(unsigned level=1;level<=3;level++) {
    e(CMP_RI(8,level));size_t other=pc++;pc=emit_gpu_write(code,pc,GPU_COLOR,level==1?0xffb740:0xff7040);
    if(level==3) {
      sr(2,S_GUIDE);e(CMP_RI(2,2));size_t guided=pc++;sr(2,S_MODE);e(CMP_RI(2,0));size_t cruising=pc++;
      hud_text(messages[level-1],716,688);size_t manual=pc++;
      code[guided]=ENC_I24(OP_JE,pc);code[cruising]=JNE(pc);hud_text("OBSTACLE AHEAD",716,688);code[manual]=JMP(pc);
    } else hud_text(messages[level-1],716,688);
    code[other]=JNE(pc);
  }
}
static void s_hud(void) {
  pc=emit_gpu_write(code,pc,GPU_DEPTH,0);panel(12,600,1012,752);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x9bd7ff);hud_text("ASTRA",24,610);hud_text("SOL SYSTEM",108,610);
  s_throttle();pc=emit_gpu_write(code,pc,GPU_COLOR,0x9bd7ff);
  hud_line(350,630,350,740);hud_line(690,630,690,740);
  hud_text("DESTINATION",374,640);sr(8,8464);s_name(8,530,640);
  s_target();sl(4,S_DIST);s_range(374,668);hud_text("CENTER",470,668);
  sr(8,S_GUIDE);e(CMP_RI(8,2));size_t rock_guidance=pc++;
  sr(8,S_MODE);e(CMP_RI(8,0));size_t local=pc++;
  pc=emit_gpu_write(code,pc,GPU_COLOR,0xffb740);hud_text("CRUISE",24,640);sl(4,S_STEP);e(MOV_RI(2,60));f(OP_ITOF,5,2);f(OP_FMUL_RR,4,5);sl(5,S_DT);f(OP_FDIV_RR,4,5);s_range(24,668);hud_text("S",144,668);size_t mode_done=pc++;
  code[local]=ENC_I24(OP_JE,pc);
  sr(8,S_MANUAL);e(CMP_RI(8,2));size_t precision=pc++;
  e(CMP_RI(8,S_CRUISE));size_t cruise_ready=pc++;
  e(CMP_RI(8,1));size_t flight=pc++;hud_text("SYSTEM",24,640);size_t name_done=pc++;
  code[flight]=JNE(pc);hud_text("FLIGHT",24,640);code[name_done]=JMP(pc);
  f(OP_ITOF,4,11);e(MOV_RI(2,30));f(OP_ITOF,5,2);f(OP_FMUL_RR,4,5);
  sr(8,S_MANUAL);e(CMP_RI(8,1));size_t slow=pc++;
  e(MOV_RI(2,S_SYSTEM_SCALE));f(OP_ITOF,5,2);f(OP_FMUL_RR,4,5);
  e(MOV_RI(2,S_SYSTEM_DIVISOR));f(OP_ITOF,5,2);f(OP_FDIV_RR,4,5);s_system_speed();size_t speed_done=pc++;
  code[slow]=JNE(pc);s_range(24,668);code[speed_done]=JMP(pc);hud_text("S",144,668);size_t manual_done=pc++;
  code[precision]=ENC_I24(OP_JE,pc);hud_text("LOCAL",24,640);f(OP_MOV_RR,7,11);e(MOV_RI(2,3));f(OP_DIV_RR,7,2);hud_number_unit(24,668,"M S");
  size_t local_done=pc++;
  code[cruise_ready]=ENC_I24(OP_JE,pc);hud_text("CRUISE READY",24,640);hud_text("T TO ENGAGE",24,668);
  size_t ready_done=pc++;
  code[rock_guidance]=ENC_I24(OP_JE,pc);hud_text("ROCK CRUISE",24,640);s_guide_speed_hud();code[ready_done]=JMP(pc);
  code[local_done]=JMP(pc);code[manual_done]=JMP(pc);code[mode_done]=JMP(pc);
  hud_text("F FLIGHT MODE",374,724);hud_text("T CRUISE",716,712);hud_text("G GROUP TAB SELECT",716,732);
  hud_text("S SCAN C CLEAR",374,700);
  hud_text("L AIM R CRUISE",716,668);
  sr(8,S_EVENT);e(CMP_RI(8,0));size_t normal=pc++;
  const char *statuses[]={"ARRIVED","ROUTE BLOCKED","SELECT CRUISE MODE","ALREADY NEARBY","SCAN FOR TARGET","SELECT SYSTEM MODE","SELECT MANUAL MODE","TARGET LOST","TARGET REACHED","ASTEROID LIMIT"};
  for(unsigned status=0;status<sizeof(statuses)/sizeof(statuses[0]);status++) {
    e(CMP_RI(8,status+1));size_t other=pc++;hud_text(statuses[status],716,640);code[other]=JNE(pc);
  }
  code[normal]=ENC_I24(OP_JE,pc);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x33ff44);hud_text("NEAR",24,700);sr(8,S_NEAREST);s_name(8,84,700);
  sl(4,S_CLEAR);sr(8,S_NEAREST);e(CMP_RI(8,S_CATALOG));size_t named_clearance=pc++;sl(4,S_ROCK_CLEAR);code[named_clearance]=JNE(pc);
  loadf(5,ZERO);f(OP_FCMP_RR,4,5);size_t positive=pc++;copy(4,5);code[positive]=ENC_I24(OP_JGE,pc);
  sl(5,S_CONST+8);f(OP_FCMP_RR,4,5);size_t km=pc++;
  e(MOV_RI(2,1000));f(OP_ITOF,5,2);f(OP_FMUL_RR,4,5);loadf(5,8512);f(OP_FADD_RR,4,5);f(OP_FTOI,7,4);
  e(CMP_RI(7,999));size_t bounded=pc++;e(MOV_RI(7,999));code[bounded]=ENC_I24(OP_JLE,pc);hud_number_unit(24,724,"M");size_t clearance_done=pc++;
  code[km]=ENC_I24(OP_JGE,pc);s_range(24,724);code[clearance_done]=JMP(pc);s_rock_warning();
  sr(8,8460);e(CMP_RI(8,0));size_t hidden=pc++;
  panel(620,30,1000,390);pc=emit_gpu_write(code,pc,GPU_COLOR,0x9bd7ff);hud_text("SOLAR SYSTEM",636,44);
  for(unsigned body=1;body<S_PRIMARY;body++) {
    pc=emit_gpu_write(code,pc,GPU_COLOR,0x193342);
    double r=8+10*body;
    for(unsigned segment=0;segment<16;segment++) {
      double a=segment*6.283185307179586/16,b=(segment+1)*6.283185307179586/16;
      hud_line((unsigned)(730+r*sin(a)),(unsigned)(190-r*cos(a)),(unsigned)(730+r*sin(b)),(unsigned)(190-r*cos(b)));
    }
  }
  for(unsigned body=0;body<S_CATALOG;body++) {
    size_t remote_moon=0;
    if(solar_parent[body]) {sr(2,S_MOON_VIS+solar_parent[body]*4);e(CMP_RI(2,0));remote_moon=pc++;}
    unsigned x=(unsigned)(730+(body?8+10*body:0)*sin(solar_angles[body]));
    unsigned y=(unsigned)(190-(body?8+10*body:0)*cos(solar_angles[body]));
    if(solar_parent[body]) {
      unsigned parent=solar_parent[body];
      x=(unsigned)(730+(8+10*parent)*sin(solar_angles[parent])+7*sin(solar_angles[body]));
      y=(unsigned)(190-(8+10*parent)*cos(solar_angles[parent])-7*cos(solar_angles[body]));
    } else if(body>=S_PRIMARY) {
      double r=48+10*(solar_orbits[body]-228e6)/(778.5e6-228e6);
      x=(unsigned)(730+r*sin(solar_angles[body]));y=(unsigned)(190-r*cos(solar_angles[body]));
    }
    pc=emit_gpu_write(code,pc,GPU_COLOR,body?0x9bd7ff:0xffb740);
    hud_line(x-2,y,x+2,y);hud_line(x,y-2,x,y+2);
    sr(8,8464);e(CMP_RI(8,body));size_t not_selected=pc++;
    pc=emit_gpu_write(code,pc,GPU_COLOR,0x33ff44);
    hud_line(x-5,y-5,x+5,y-5);hud_line(x-5,y+5,x+5,y+5);code[not_selected]=JNE(pc);
    if(remote_moon)code[remote_moon]=ENC_I24(OP_JE,pc);
  }
  sr(8,8464);sr(6,S_GROUP);
  for(unsigned group=0;group<S_GROUPS;group++) {
    e(CMP_RI(6,group));size_t skip=pc++;
    pc=emit_gpu_write(code,pc,GPU_COLOR,0x9bd7ff);hud_text(solar_groups[group].name,636,68);
    for(unsigned item=0;item<solar_groups[group].count;item++) {
      unsigned b=solar_groups[group].bodies[item],y=100+item*23;
      pc=emit_gpu_write(code,pc,GPU_COLOR,0x9bd7ff);
      e(CMP_RI(8,b));size_t other=pc++;pc=emit_gpu_write(code,pc,GPU_COLOR,0x33ff44);code[other]=JNE(pc);
      hud_text(solar_names[b],850,y);
    }
    if(group==S_GROUPS-1)hud_text("BELT DESTINATIONS",636,295);
    else if(group) {hud_text("PLANET AND MOONS",636,295);}
    code[skip]=JNE(pc);
  }
  /* Main belt: approximate 2.2--3.2 AU annulus on the compressed chart. */
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x8c7852);
  for(unsigned i=0;i<72;i++) {
    double a=i*6.283185307179586/72;
    double r=48+10*((2.2+(i%3)*.5)*149.6e6-228e6)/(778.5e6-228e6);
    unsigned x=(unsigned)(730+r*sin(a)),y=(unsigned)(190-r*cos(a));hud_line(x,y,x+1,y);
  }
  hud_text("MAIN BELT",636,340);
  /* Position marker uses the same piecewise radial compression as orbit rings. */
  sl(4,S_POS);f(OP_FMUL_RR,4,4);sl(5,S_POS+16);f(OP_FMUL_RR,5,5);f(OP_FADD_RR,4,5);sroot();copy(9,4);
  sl(5,S_POS);f(OP_FDIV_RR,5,4);ss(5,8848);sl(5,S_POS+16);f(OP_FDIV_RR,5,4);ss(5,8856);
  size_t plot_exits[S_PRIMARY-1];
  for(unsigned body=1;body<S_PRIMARY;body++) {
    sl(5,S_BODY+body*64+56);f(OP_FCMP_RR,9,5);size_t farther=pc++;
    sl(6,S_BODY+(body-1)*64+56);copy(4,9);f(OP_FSUB_RR,4,6);f(OP_FSUB_RR,5,6);f(OP_FDIV_RR,4,5);
    sl(5,S_BODY+body*64+48);sl(6,S_BODY+(body-1)*64+48);f(OP_FSUB_RR,5,6);f(OP_FMUL_RR,4,5);f(OP_FADD_RR,4,6);ss(4,8864);
    plot_exits[body-1]=pc++;code[farther]=JG(pc);
  }
  e(MOV_RI(2,98));f(OP_ITOF,4,2);ss(4,8864);
  for(unsigned body=0;body<S_PRIMARY-1;body++) code[plot_exits[body]]=JMP(pc);
  sl(4,8848);sl(5,8864);f(OP_FMUL_RR,4,5);e(MOV_RI(2,730));f(OP_ITOF,5,2);f(OP_FADD_RR,4,5);f(OP_FTOI,6,4);
  sl(4,8856);sl(5,8864);f(OP_FMUL_RR,4,5);e(MOV_RI(2,190));f(OP_ITOF,5,2);f(OP_FSUB_RR,5,4);f(OP_FTOI,7,5);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x33ff44);gpu_reg(GPU_X,6);gpu_reg(GPU_Y,7);e(ADD_RI(6,3));gpu_reg(GPU_X2,6);gpu_reg(GPU_Y2,7);pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_LINE);
  e(SUB_RI(6,3));gpu_reg(GPU_X,6);gpu_reg(GPU_Y,7);
  trig(9,0,1);trig(10,2,3);e(MOV_RI(2,18));f(OP_ITOF,5,2);
  copy(4,0);f(OP_FMUL_RR,4,3);f(OP_FMUL_RR,4,5);f(OP_FTOI,2,4);f(OP_ADD_RR,2,6);gpu_reg(GPU_X2,2);
  copy(4,1);f(OP_FMUL_RR,4,3);f(OP_FMUL_RR,4,5);f(OP_FTOI,2,4);f(OP_MOV_RR,3,7);f(OP_SUB_RR,3,2);gpu_reg(GPU_Y2,3);
  pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_LINE);
  hud_text("ASTRA GREEN",636,320);pc=emit_gpu_write(code,pc,GPU_COLOR,0x9bd7ff);hud_text("G GROUP TAB SELECT",636,360);code[hidden]=ENC_I24(OP_JE,pc);
  s_instruments();
}
/* Regional generation uses only guest instructions at runtime. */
#include "belt.inc"

static void s_nearby(void) {
  trig(9,0,1);trig(10,2,3);
  copy(4,0);f(OP_FMUL_RR,4,3);ss(4,S_WARN_DIR);
  loadf(4,ZERO);f(OP_FSUB_RR,4,2);ss(4,S_WARN_DIR+8);
  copy(4,1);f(OP_FMUL_RR,4,3);ss(4,S_WARN_DIR+16);
  sr(8,S_GUIDE);e(CMP_RI(8,2));size_t manual=pc++;
  for(unsigned k=0;k<3;k++){sl(4,S_SCAN_DIR+k*8);sl(5,S_SCAN_DIST);f(OP_FDIV_RR,4,5);ss(4,S_WARN_DIR+k*8);}
  code[manual]=JNE(pc);
  sl(4,S_STEP);sl(5,S_DT);f(OP_FDIV_RR,4,5);e(MOV_RI(2,300));f(OP_ITOF,5,2);f(OP_FMUL_RR,4,5);
  sr(8,S_GUIDE);e(CMP_RI(8,2));size_t guided_speed=pc++;
  sr(8,S_MODE);e(CMP_RI(8,0));size_t cruising=pc++;e(CMP_RI(11,0));size_t moving=pc++;loadf(4,ZERO);size_t speed_ready=pc++;
  code[guided_speed]=ENC_I24(OP_JE,pc);sl(4,S_GUIDE_SPEED);e(MOV_RI(2,5));f(OP_ITOF,5,2);f(OP_FMUL_RR,4,5);
  code[cruising]=JNE(pc);code[moving]=JNE(pc);code[speed_ready]=JMP(pc);ss(4,S_WARN_REACH);
  sl(4,S_CONST+56);ss(4,S_CLEAR);si(S_NEAREST,0);
  for(unsigned b=0;b<S_CATALOG;b++) {
    s_delta(b);sroot();sl(5,S_BODY+b*64+32);f(OP_FSUB_RR,4,5);sl(5,S_CLEAR);f(OP_FCMP_RR,4,5);size_t farther=pc++;
    ss(4,S_CLEAR);si(S_NEAREST,b);code[farther]=ENC_I24(OP_JGE,pc);
  }
  b_nearest();sr(8,S_EVENT);e(CMP_RI(8,10));size_t no_limit=pc++;
  sl(4,S_ROCK_CLEAR);sl(5,S_CONST+120);f(OP_FCMP_RR,4,5);size_t close=pc++;si(S_EVENT,0);
  code[close]=ENC_I24(OP_JLE,pc);code[no_limit]=JNE(pc);
}

/* Bearings use the same world axes as flight: 000 = +Z, 090 = +X.
 * The horizontal belt course is a counterclockwise tangent around Sol. */
static void s_angle(unsigned address,unsigned x,unsigned y,bool pitch) {
  sr(7,address);
  if(pitch) {
    e(CMP_RI(7,128));size_t down=pc++;
    e(MOV_RI(2,256));f(OP_SUB_RR,2,7);f(OP_MOV_RR,7,2);
    hud_text("UP",x+54,y);size_t done=pc++;
    code[down]=JL(pc);hud_text("DN",x+54,y);code[done]=JMP(pc);
  }
  e(MOV_RI(2,360));f(OP_MUL_RR,7,2);e(MOV_RI(2,256));f(OP_DIV_RR,7,2);
  hud_number_unit(x,y,pitch?"":"DEG");
}
static void s_store_bearing(unsigned vector,unsigned yaw,unsigned pitch) {
  s_bearing(vector,12,13);
  e(MOV_RI(0,yaw));f(OP_STORE32,12,0);e(MOV_RI(0,pitch));f(OP_STORE32,13,0);
}
static void s_instruments(void) {
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x9bd7ff);panel(12,24,470,90);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x9bd7ff);
  trig(9,0,1);trig(10,2,3);
  copy(4,0);f(OP_FMUL_RR,4,3);ss(4,S_BELT_DIR);
  loadf(4,ZERO);f(OP_FSUB_RR,4,2);ss(4,S_BELT_DIR+8);
  copy(4,1);f(OP_FMUL_RR,4,3);ss(4,S_BELT_DIR+16);
  s_store_bearing(S_BELT_DIR,S_HEADING,S_PITCH);
  hud_text("HDG",24,36);s_angle(S_HEADING,84,36,false);
  hud_text("PITCH",240,36);s_angle(S_PITCH,312,36,true);
  sl(4,S_POS);f(OP_FMUL_RR,4,4);sl(5,S_POS+16);f(OP_FMUL_RR,5,5);f(OP_FADD_RR,4,5);
  si(S_IN_BELT,0);
  sl(5,B_CONST+40);f(OP_FCMP_RR,4,5);size_t inner=pc++;
  sl(5,B_CONST+48);f(OP_FCMP_RR,4,5);size_t outer=pc++;
  sl(5,S_POS+8);f(OP_FMUL_RR,5,5);sl(6,B_CONST+56);f(OP_FCMP_RR,5,6);size_t high=pc++;
  si(S_IN_BELT,1);
  code[inner]=JL(pc);code[outer]=JG(pc);code[high]=JG(pc);
  sroot();ss(4,S_SUN_DIST);
  sr(8,S_IN_BELT);e(CMP_RI(8,0));size_t outside=pc++;
  hud_text("IN MAIN BELT",24,64);size_t belt_done=pc++;
  code[outside]=ENC_I24(OP_JE,pc);hud_text("OUTSIDE BELT",24,64);code[belt_done]=JMP(pc);
  sl(4,S_POS+16);ss(4,S_BELT_DIR);loadf(4,ZERO);ss(4,S_BELT_DIR+8);
  sl(5,S_POS);f(OP_FSUB_RR,4,5);ss(4,S_BELT_DIR+16);
  s_store_bearing(S_BELT_DIR,S_BELT_YAW,S_SCAN_PITCH);
  hud_text("COURSE",240,64);s_angle(S_BELT_YAW,324,64,false);
  sr(8,S_SCAN_STATE);e(CMP_RI(8,0));size_t idle=pc++;
  panel(12,100,414,164);pc=emit_gpu_write(code,pc,GPU_COLOR,0x9bd7ff);
  sr(8,S_SCAN_FOUND);e(CMP_RI(8,0));size_t none=pc++;
  s_store_bearing(S_SCAN_DIR,S_SCAN_YAW,S_SCAN_PITCH);
  pc=emit_gpu_write(code,pc,GPU_COLOR,0x33ff44);
  hud_text("ROCK",24,112);sl(4,S_SCAN_DIST);sl(5,S_CONST+8);f(OP_FCMP_RR,4,5);size_t kilometres=pc++;
  e(MOV_RI(2,1000));f(OP_ITOF,5,2);f(OP_FMUL_RR,4,5);f(OP_FTOI,7,4);hud_number_unit(84,112,"M");size_t rock_range_done=pc++;
  code[kilometres]=ENC_I24(OP_JGE,pc);s_range(84,112);code[rock_range_done]=JMP(pc);
  const char *guide_labels[]={"LOCKED","ALIGN MANUAL","CRUISE"};sr(8,S_GUIDE);
  for(unsigned mode=0;mode<3;mode++){e(CMP_RI(8,mode));size_t other=pc++;hud_text(guide_labels[mode],252,112);code[other]=JNE(pc);}
  hud_text("HDG",24,140);s_angle(S_SCAN_YAW,84,140,false);
  hud_text("PITCH",216,140);s_angle(S_SCAN_PITCH,288,140,true);
  size_t scan_done=pc++;
  code[none]=ENC_I24(OP_JE,pc);sr(8,S_SCAN_STATE);e(CMP_RI(8,3));size_t empty=pc++;
  hud_text("CONTACT OUT OF RANGE",24,112);size_t message_done=pc++;
  code[empty]=JNE(pc);hud_text("NO CONTACT",24,112);code[message_done]=JMP(pc);
  hud_text("S RESCAN 50 MKM PULSE",24,140);code[scan_done]=JMP(pc);code[idle]=ENC_I24(OP_JE,pc);
}

static void solar_coarse_data(unsigned body,const double center[3]) {
  const double phi=1.618033988749895;
  const double vertices[12][3]={{-1,phi,0},{1,phi,0},{-1,-phi,0},{1,-phi,0},{0,-1,phi},{0,1,phi},{0,-1,-phi},{0,1,-phi},{phi,0,-1},{phi,0,1},{-phi,0,-1},{-phi,0,1}};
  const unsigned faces[20][3]={{0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},{1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},{3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},{4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}};
  for(unsigned face=0;face<20;face++) {
    unsigned address=S_COARSE+body*1200+face*60;double normal[3]={0};
    for(unsigned vertex=0;vertex<3;vertex++) for(unsigned k=0;k<3;k++) {
      unsigned vi=faces[face][vertex];
      double shape=body<S_CATALOG?1:.72+.28*((vi*17+body*13)%19)/18.0;
      double component=shape*vertices[vi][k]/sqrt(1+phi*phi);
      normal[k]+=component;s_data32(address+vertex*12+k*4,(float)(component*solar_radii[body]));
    }
    double length=sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
    for(unsigned k=0;k<3;k++) {
      s_data32(address+36+k*4,(float)(normal[k]/length));
      s_data32(address+48+k*4,body?(float)(-center[k]/hypot(center[0],center[2])):0);
    }
  }
}
static void solar_init_data(void) {
  memset(solar_centers,0,sizeof(solar_centers));
  memset(solar_data,0,sizeof(solar_data));memcpy(solar_data,flight_data,sizeof(flight_data));
  memcpy(solar_data+16384,solid_data,20);
  s_data32(12800,1e6f);s_data32(12804,1e9f);
  s_data32(16384,1e9f);
  s_data32(8520,(float)sqrt(1+1.28*1.28));s_data32(8524,(float)sqrt(1+.96*.96));
  s_data32(NEAR,1);s_data32(CHASE,0);s_data32(8512,.5);s_data32(8516,-2);
  /* LOCAL step scale: 100 m/s divided by 300 throttle steps, 60 Hz, 1000 m/km. */
  const double constants[]={149.6e6/60,1,.5,1,1000,1e6,149.6e6,1e15,.05,.001,8*149.6e6,1.0/180000,50e6,
    .001,S_SYSTEM_KM_S,.1,.05,10,.01};
  for(unsigned k=0;k<sizeof(constants)/sizeof(constants[0]);k++) s_data64(S_CONST+k*8,constants[k]);
  for(unsigned parent=1;parent<S_PRIMARY;parent++) {
    double radius=0;
    for(unsigned b=0;b<S_CATALOG;b++)if(solar_parent[b]==parent)radius=fmax(radius,solar_orbits[b]*3);
    s_data64(S_SYSTEM_RADIUS+parent*8,fmax(100000,radius));
  }
  for(unsigned b=0;b<S_COUNT;b++) {
    double *center=solar_centers[b];
    center[0]=solar_orbits[b]*sin(solar_angles[b])+solar_centers[solar_parent[b]][0];
    center[1]=0;center[2]=solar_orbits[b]*cos(solar_angles[b])+solar_centers[solar_parent[b]][2];
    if(b==S_CATALOG) {solar_radii[b]=1;center[0]=3e8;}
    solar_coarse_data(b,center);
    for(unsigned k=0;k<3;k++) s_data64(S_BODY+b*64+k*8,center[k]);
    s_data64(S_BODY+b*64+24,solar_radii[b]);
    s_data64(S_BODY+b*64+48,b?8+10*b:0);
    s_data64(S_BODY+b*64+56,solar_orbits[b]);
    s_data64(S_BODY+b*64+32,solar_radii[b]+(b>=S_PRIMARY?1:b?100:10000));
    s_data64(S_BODY+b*64+40,solar_radii[b]+fmax(solar_radii[b]*.2,b>=S_PRIMARY?20:1000));
    if(b<S_CATALOG)s_data64(S_NEAR_RADIUS+b*8,fmax(5*solar_radii[b],2*(solar_radii[b]+fmax(solar_radii[b]*.2,b>=S_PRIMARY?20:1000))));
    for(unsigned face=0;face<80;face++) {
      unsigned source=solid_faces[face][0]-16384,dest=30000+b*4800+face*60;
      for(unsigned k=0;k<15;k++) {
        uint32_t bits=0;for(unsigned j=0;j<4;j++) bits|=(uint32_t)solid_data[source+k*4+j]<<(j*8);
        float v;memcpy(&v,&bits,4);
        if(k<9) {double offset=(k%3==0?-80:k%3==1?20:220);v=(float)((v-offset)/60*solar_radii[b]);}
        if(k>=12) v=b?(float)(-center[k-12]/hypot(center[0],center[2])):0;
        s_data32(dest+k*4,v);
      }
    }
  }
  b_init_data();
  s_data64(S_DT,1);
  s_data64(S_POS,0);s_data64(S_POS+8,0);s_data64(S_POS+16,149.6e6-solar_radii[3]*3);
}
static void s_controls(void) {
  s_press(KEY_M,0);s_press(KEY_G,4);s_press(KEY_TAB,1);s_press(KEY_F,3);s_press(KEY_T,2);
  s_press(KEY_S,5);s_press(KEY_C,6);
  s_press(KEY_L,7);s_press(KEY_R,8);
  /* Fresh manual input overrides rock cruise. Keys held before R are ignored
   * until released, including after automatic arrival. */
  const unsigned overrides[]={KEY_UP,KEY_LEFT,KEY_DOWN,KEY_RIGHT,KEY_X,KEY_Z};
  for(unsigned k=0;k<sizeof(overrides)/sizeof(overrides[0]);k++) {
    unsigned input=overrides[k];sr(8,S_GUIDE);e(CMP_RI(8,2));size_t manual=pc++;
    e(ENC_RI(OP_LUI,0,INPUT_BASE>>12));e(ADD_RI(0,INPUT_PRESSED+input*4));f(OP_LOAD32,2,0);size_t check=pc++;
    code[manual]=JNE(pc);if(input==KEY_Z)e(MOV_RI(2,0));else s_pilot_key(input,2);
    code[check]=JMP(pc);e(CMP_RI(2,0));size_t idle=pc++;
    s_ack_manual_press(input);e(MOV_RI(0,S_INPUT_BLOCK+input*4));e(MOV_RI(1,0));f(OP_STORE32,1,0);s_cancel_guide();code[idle]=ENC_I24(OP_JE,pc);
  }
  /* Brake cancels travel immediately; steering is available in local flight. */
  s_pilot_key(KEY_X,2);e(CMP_RI(2,0));size_t no_brake=pc++;si(S_MODE,0);si(S_EVENT,0);code[no_brake]=ENC_I24(OP_JE,pc);
  sr(8,S_MODE);e(CMP_RI(8,0));size_t auto_course=pc++;
  sr(8,S_GUIDE);e(CMP_RI(8,2));size_t rock_course=pc++;
  trig(10,2,3);loadf(5,ZERO);f(OP_FCMP_RR,3,5);size_t inverted=pc++;
  s_pilot_control(9,KEY_LEFT,KEY_RIGHT,0,255,true);size_t yaw_done=pc++;
  code[inverted]=JL(pc);s_pilot_control(9,KEY_RIGHT,KEY_LEFT,0,255,true);code[yaw_done]=JMP(pc);
  s_pilot_control(10,KEY_UP,KEY_DOWN,0,255,true);
  sr(8,S_MANUAL);e(CMP_RI(8,S_CRUISE));size_t armed=pc++;
  s_pilot_control(11,KEY_X,KEY_Z,0,300,false);code[armed]=ENC_I24(OP_JE,pc);
  code[auto_course]=JNE(pc);code[rock_course]=ENC_I24(OP_JE,pc);
}
static prog_t solar_build(bool physics_only,bool labels) {
  solar_init_data();pc=0;e(MOV_RI(9,0));e(MOV_RI(10,0));e(MOV_RI(11,0));si(8460,1);si(8464,4);si(S_MANUAL,0);si(S_GROUP,0);
  size_t loop=pc;
  b_update();b_scan();s_context();s_nearby();
  if(!physics_only) {
    /* All geometry shares one reciprocal-depth scale per frame. Short-range
     * rock inspection needs smaller Q values to fit the existing integer ABI. */
    sl(4,S_ROCK_CLEAR);sl(5,S_CONST+136);f(OP_FCMP_RR,4,5);size_t distant_depth=pc++;
    loadf(4,12800);storef(4,16384);size_t depth_ready=pc++;
    code[distant_depth]=JG(pc);loadf(4,12804);storef(4,16384);code[depth_ready]=JMP(pc);
    trig(9,0,1);trig(10,2,3);
    pc=emit_gpu_write(code,pc,GPU_COLOR,0);pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_CLEAR);pc=emit_gpu_write(code,pc,GPU_DEPTH,1);
    for(unsigned b=0;b<S_CATALOG;b++) {
      si(s_lod_address(b),3);size_t rejected[5];s_frustum(b,rejected);
      /* Approximate projected radius f*r/z: point below 1 px, icosahedron
       * from 1 to 16 px, existing detailed sphere above 16 px. */
      s_camera(b,0);sl(5,S_BODY+b*64+24);e(MOV_RI(2,400));f(OP_ITOF,6,2);f(OP_FMUL_RR,6,5);
      f(OP_FCMP_RR,7,6);size_t point=pc++;
      e(MOV_RI(2,25));f(OP_ITOF,6,2);f(OP_FMUL_RR,6,5);f(OP_FCMP_RR,7,6);size_t coarse=pc++;
      si(s_lod_address(b),2);for(unsigned face=0;face<(b<S_CATALOG?80u:20u);face++) s_face(b,face,b>=S_CATALOG);size_t done=pc++;
      code[coarse]=JG(pc);si(s_lod_address(b),1);for(unsigned face=0;face<20;face++) s_face(b,face,true);size_t coarse_done=pc++;
      code[point]=JG(pc);si(s_lod_address(b),0);if(b<S_CATALOG)s_point(b);code[done]=JMP(pc);code[coarse_done]=JMP(pc);
      for(unsigned plane=0;plane<5;plane++) code[rejected[plane]]=JL(pc);
    }
    b_render();if(labels)s_labels();s_hud();
    pc=emit_gpu_write(code,pc,GPU_COLOR,0x33ff44);hud_line(490,470,512,445);hud_line(512,445,534,470);hud_line(534,470,490,470);
    pc=emit_gpu_write(code,pc,GPU_COMMAND,GPU_PRESENT);e(ENC_OP(OP_WAIT_FRAME));
  }
  if(!physics_only) {
    /* Integrate cruise against guest time, including expensive rendered frames.
     * Unsigned subtraction handles the low clock word wrapping. */
    e(ENC_RI(OP_LUI,0,CLOCK_BASE>>12));f(OP_LOAD32,3,0);
    sr(2,S_LAST_TICK);f(OP_MOV_RR,5,3);f(OP_SUB_RR,5,2);
    e(MOV_RI(0,S_LAST_TICK));f(OP_STORE32,3,0);
    f(OP_ITOF,4,5);e(MOV_RI(2,FRAME_TICKS));f(OP_ITOF,5,2);f(OP_FDIV_RR,4,5);ss(4,S_DT);
  }
  s_controls();s_move();
  if(physics_only) e(ENC_OP(OP_YIELD));
  e(JMP(loop));
  return (prog_t){.code=code,.codelen=pc,.data=solar_data,.datalen=sizeof(solar_data)};
}
prog_t solar_flight(void) { return solar_build(false,true); }
#ifdef CGVM_TEST
prog_t solar_physics_probe(void) { return solar_build(true,false); }
prog_t solar_no_labels_probe(void) { return solar_build(false,false); }
prog_t solar_region_probe(void) {
  solar_init_data();pc=0;size_t loop=pc;b_generate();e(ENC_OP(OP_YIELD));e(JMP(loop));
  return (prog_t){.code=code,.codelen=pc,.data=solar_data,.datalen=sizeof(solar_data)};
}
#endif
