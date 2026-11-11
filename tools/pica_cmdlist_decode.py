#!/usr/bin/env python3
"""Decode a PICA200 GPU command list (the buffer given to GX ProcessCommandList).

Format source: libctru source/gpu/gpu.c (GPUCMD_AddInternal) at commit 62b7c42;
register names from libctru include/3ds/gpu/registers.h (same commit), plus the
following ids that libctru leaves unnamed or misplaces and that 3dbrew GPU/Internal Registers
names: 0x04C DEPTHMAP_TRANSFORM_ENABLE, 0x0AC/0x0AD TEXUNIT3_PROCTEX4/5, 0x0E2/0x0E3
FOG_LINEAR_SLOPE/OFFSET (see ctr/hardware/gpu-internal-registers.md).

VERIFICATION STATUS: tested only on SYNTHETIC command lists that this file
builds itself (python3 -I tools/pica_cmdlist_decode.py --selftest). It has
never been run on a capture from a real console or emulator; no such capture
exists on this wiki. Treat its warnings as hints, not as proof of a bug.

Usage:
  python3 -I tools/pica_cmdlist_decode.py LIST.bin         # little-endian u32 words
  python3 -I tools/pica_cmdlist_decode.py --hex LIST.txt   # whitespace-separated hex words
  python3 -I tools/pica_cmdlist_decode.py --selftest
"""
import io
import struct
import sys

REG_FINALIZE = 0x010
REG_ATTRIBBUFFERS_LOC = 0x200
REG_FORMAT_LOW, REG_FORMAT_HIGH = 0x201, 0x202
REG_INDEXBUFFER_CONFIG = 0x227
REG_NUMVERTICES = 0x228
REG_DRAWARRAYS, REG_DRAWELEMENTS = 0x22E, 0x22F

REG_NAMES = {
    0x000:"0000",0x001:"0001",0x002:"0002",0x003:"0003",0x004:"0004",0x005:"0005",0x006:"0006",
    0x007:"0007",0x008:"0008",0x009:"0009",0x00A:"000A",0x00B:"000B",0x00C:"000C",0x00D:"000D",
    0x00E:"000E",0x00F:"000F",0x010:"FINALIZE",0x011:"0011",0x012:"0012",0x013:"0013",0x014:"0014",
    0x015:"0015",0x016:"0016",0x017:"0017",0x018:"0018",0x019:"0019",0x01A:"001A",0x01B:"001B",
    0x01C:"001C",0x01D:"001D",0x01E:"001E",0x01F:"001F",0x020:"0020",0x021:"0021",0x022:"0022",
    0x023:"0023",0x024:"0024",0x025:"0025",0x026:"0026",0x027:"0027",0x028:"0028",0x029:"0029",
    0x02A:"002A",0x02B:"002B",0x02C:"002C",0x02D:"002D",0x02E:"002E",0x02F:"002F",0x030:"0030",
    0x031:"0031",0x032:"0032",0x033:"0033",0x034:"0034",0x035:"0035",0x036:"0036",0x037:"0037",
    0x038:"0038",0x039:"0039",0x03A:"003A",0x03B:"003B",0x03C:"003C",0x03D:"003D",0x03E:"003E",
    0x03F:"003F",0x040:"FACECULLING_CONFIG",0x041:"VIEWPORT_WIDTH",0x042:"VIEWPORT_INVW",
    0x043:"VIEWPORT_HEIGHT",0x044:"VIEWPORT_INVH",0x045:"0045",0x046:"0046",0x047:"FRAGOP_CLIP",
    0x048:"FRAGOP_CLIP_DATA0",0x049:"FRAGOP_CLIP_DATA1",0x04A:"FRAGOP_CLIP_DATA2",
    0x04B:"FRAGOP_CLIP_DATA3",0x04C:"DEPTHMAP_TRANSFORM_ENABLE",0x04D:"DEPTHMAP_SCALE",0x04E:"DEPTHMAP_OFFSET",
    0x04F:"SH_OUTMAP_TOTAL",0x050:"SH_OUTMAP_O0",0x051:"SH_OUTMAP_O1",0x052:"SH_OUTMAP_O2",
    0x053:"SH_OUTMAP_O3",0x054:"SH_OUTMAP_O4",0x055:"SH_OUTMAP_O5",0x056:"SH_OUTMAP_O6",
    0x057:"0057",0x058:"0058",0x059:"0059",0x05A:"005A",0x05B:"005B",0x05C:"005C",0x05D:"005D",
    0x05E:"005E",0x05F:"005F",0x060:"0060",0x061:"EARLYDEPTH_FUNC",0x062:"EARLYDEPTH_TEST1",
    0x063:"EARLYDEPTH_CLEAR",0x064:"SH_OUTATTR_MODE",0x065:"SCISSORTEST_MODE",
    0x066:"SCISSORTEST_POS",0x067:"SCISSORTEST_DIM",0x068:"VIEWPORT_XY",0x069:"0069",
    0x06A:"EARLYDEPTH_DATA",0x06B:"006B",0x06C:"006C",0x06D:"DEPTHMAP_ENABLE",0x06E:"RENDERBUF_DIM",
    0x06F:"SH_OUTATTR_CLOCK",0x070:"0070",0x071:"0071",0x072:"0072",0x073:"0073",0x074:"0074",
    0x075:"0075",0x076:"0076",0x077:"0077",0x078:"0078",0x079:"0079",0x07A:"007A",0x07B:"007B",
    0x07C:"007C",0x07D:"007D",0x07E:"007E",0x07F:"007F",0x080:"TEXUNIT_CONFIG",
    0x081:"TEXUNIT0_BORDER_COLOR",0x082:"TEXUNIT0_DIM",0x083:"TEXUNIT0_PARAM",0x084:"TEXUNIT0_LOD",
    0x085:"TEXUNIT0_ADDR1",0x086:"TEXUNIT0_ADDR2",0x087:"TEXUNIT0_ADDR3",0x088:"TEXUNIT0_ADDR4",
    0x089:"TEXUNIT0_ADDR5",0x08A:"TEXUNIT0_ADDR6",0x08B:"TEXUNIT0_SHADOW",0x08C:"008C",0x08D:"008D",
    0x08E:"TEXUNIT0_TYPE",0x08F:"LIGHTING_ENABLE0",0x090:"0090",0x091:"TEXUNIT1_BORDER_COLOR",
    0x092:"TEXUNIT1_DIM",0x093:"TEXUNIT1_PARAM",0x094:"TEXUNIT1_LOD",0x095:"TEXUNIT1_ADDR",
    0x096:"TEXUNIT1_TYPE",0x097:"0097",0x098:"0098",0x099:"TEXUNIT2_BORDER_COLOR",
    0x09A:"TEXUNIT2_DIM",0x09B:"TEXUNIT2_PARAM",0x09C:"TEXUNIT2_LOD",0x09D:"TEXUNIT2_ADDR",
    0x09E:"TEXUNIT2_TYPE",0x09F:"009F",0x0A0:"00A0",0x0A1:"00A1",0x0A2:"00A2",0x0A3:"00A3",
    0x0A4:"00A4",0x0A5:"00A5",0x0A6:"00A6",0x0A7:"00A7",0x0A8:"TEXUNIT3_PROCTEX0",
    0x0A9:"TEXUNIT3_PROCTEX1",0x0AA:"TEXUNIT3_PROCTEX2",0x0AB:"TEXUNIT3_PROCTEX3",0x0AC:"TEXUNIT3_PROCTEX4",0x0AD:"TEXUNIT3_PROCTEX5",0x0AE:"00AE",
    0x0AF:"PROCTEX_LUT",0x0B0:"PROCTEX_LUT_DATA0",0x0B1:"PROCTEX_LUT_DATA1",
    0x0B2:"PROCTEX_LUT_DATA2",0x0B3:"PROCTEX_LUT_DATA3",0x0B4:"PROCTEX_LUT_DATA4",
    0x0B5:"PROCTEX_LUT_DATA5",0x0B6:"PROCTEX_LUT_DATA6",0x0B7:"PROCTEX_LUT_DATA7",0x0B8:"00B8",
    0x0B9:"00B9",0x0BA:"00BA",0x0BB:"00BB",0x0BC:"00BC",0x0BD:"00BD",0x0BE:"00BE",0x0BF:"00BF",
    0x0C0:"TEXENV0_SOURCE",0x0C1:"TEXENV0_OPERAND",0x0C2:"TEXENV0_COMBINER",0x0C3:"TEXENV0_COLOR",
    0x0C4:"TEXENV0_SCALE",0x0C5:"00C5",0x0C6:"00C6",0x0C7:"00C7",0x0C8:"TEXENV1_SOURCE",
    0x0C9:"TEXENV1_OPERAND",0x0CA:"TEXENV1_COMBINER",0x0CB:"TEXENV1_COLOR",0x0CC:"TEXENV1_SCALE",
    0x0CD:"00CD",0x0CE:"00CE",0x0CF:"00CF",0x0D0:"TEXENV2_SOURCE",0x0D1:"TEXENV2_OPERAND",
    0x0D2:"TEXENV2_COMBINER",0x0D3:"TEXENV2_COLOR",0x0D4:"TEXENV2_SCALE",0x0D5:"00D5",0x0D6:"00D6",
    0x0D7:"00D7",0x0D8:"TEXENV3_SOURCE",0x0D9:"TEXENV3_OPERAND",0x0DA:"TEXENV3_COMBINER",
    0x0DB:"TEXENV3_COLOR",0x0DC:"TEXENV3_SCALE",0x0DD:"00DD",0x0DE:"00DE",0x0DF:"00DF",
    0x0E0:"TEXENV_UPDATE_BUFFER",0x0E1:"FOG_COLOR",0x0E2:"FOG_LINEAR_SLOPE",0x0E3:"FOG_LINEAR_OFFSET",
    0x0E4:"GAS_ATTENUATION",0x0E5:"GAS_ACCMAX",0x0E6:"FOG_LUT_INDEX",0x0E7:"00E7",
    0x0E8:"FOG_LUT_DATA0",0x0E9:"FOG_LUT_DATA1",0x0EA:"FOG_LUT_DATA2",0x0EB:"FOG_LUT_DATA3",
    0x0EC:"FOG_LUT_DATA4",0x0ED:"FOG_LUT_DATA5",0x0EE:"FOG_LUT_DATA6",0x0EF:"FOG_LUT_DATA7",
    0x0F0:"TEXENV4_SOURCE",0x0F1:"TEXENV4_OPERAND",0x0F2:"TEXENV4_COMBINER",0x0F3:"TEXENV4_COLOR",
    0x0F4:"TEXENV4_SCALE",0x0F5:"00F5",0x0F6:"00F6",0x0F7:"00F7",0x0F8:"TEXENV5_SOURCE",
    0x0F9:"TEXENV5_OPERAND",0x0FA:"TEXENV5_COMBINER",0x0FB:"TEXENV5_COLOR",0x0FC:"TEXENV5_SCALE",
    0x0FD:"TEXENV_BUFFER_COLOR",0x0FE:"00FE",0x0FF:"00FF",0x100:"COLOR_OPERATION",
    0x101:"BLEND_FUNC",0x102:"LOGIC_OP",0x103:"BLEND_COLOR",0x104:"FRAGOP_ALPHA_TEST",
    0x105:"STENCIL_TEST",0x106:"STENCIL_OP",0x107:"DEPTH_COLOR_MASK",0x108:"0108",0x109:"0109",
    0x10A:"010A",0x10B:"010B",0x10C:"010C",0x10D:"010D",0x10E:"010E",0x10F:"010F",
    0x110:"FRAMEBUFFER_INVALIDATE",0x111:"FRAMEBUFFER_FLUSH",0x112:"COLORBUFFER_READ",
    0x113:"COLORBUFFER_WRITE",0x114:"DEPTHBUFFER_READ",0x115:"DEPTHBUFFER_WRITE",
    0x116:"DEPTHBUFFER_FORMAT",0x117:"COLORBUFFER_FORMAT",0x118:"EARLYDEPTH_TEST2",0x119:"0119",
    0x11A:"011A",0x11B:"FRAMEBUFFER_BLOCK32",0x11C:"DEPTHBUFFER_LOC",0x11D:"COLORBUFFER_LOC",
    0x11E:"FRAMEBUFFER_DIM",0x11F:"011F",0x120:"GAS_LIGHT_XY",0x121:"GAS_LIGHT_Z",
    0x122:"GAS_LIGHT_Z_COLOR",0x123:"GAS_LUT_INDEX",0x124:"GAS_LUT_DATA",
    0x125:"GAS_ACCMAX_FEEDBACK",0x126:"GAS_DELTAZ_DEPTH",0x127:"0127",0x128:"0128",0x129:"0129",
    0x12A:"012A",0x12B:"012B",0x12C:"012C",0x12D:"012D",0x12E:"012E",0x12F:"012F",
    0x130:"FRAGOP_SHADOW",0x131:"0131",0x132:"0132",0x133:"0133",0x134:"0134",0x135:"0135",
    0x136:"0136",0x137:"0137",0x138:"0138",0x139:"0139",0x13A:"013A",0x13B:"013B",0x13C:"013C",
    0x13D:"013D",0x13E:"013E",0x13F:"013F",0x140:"LIGHT0_SPECULAR0",0x141:"LIGHT0_SPECULAR1",
    0x142:"LIGHT0_DIFFUSE",0x143:"LIGHT0_AMBIENT",0x144:"LIGHT0_XY",0x145:"LIGHT0_Z",
    0x146:"LIGHT0_SPOTDIR_XY",0x147:"LIGHT0_SPOTDIR_Z",0x148:"0148",0x149:"LIGHT0_CONFIG",
    0x14A:"LIGHT0_ATTENUATION_BIAS",0x14B:"LIGHT0_ATTENUATION_SCALE",0x14C:"014C",0x14D:"014D",
    0x14E:"014E",0x14F:"014F",0x150:"LIGHT1_SPECULAR0",0x151:"LIGHT1_SPECULAR1",
    0x152:"LIGHT1_DIFFUSE",0x153:"LIGHT1_AMBIENT",0x154:"LIGHT1_XY",0x155:"LIGHT1_Z",
    0x156:"LIGHT1_SPOTDIR_XY",0x157:"LIGHT1_SPOTDIR_Z",0x158:"0158",0x159:"LIGHT1_CONFIG",
    0x15A:"LIGHT1_ATTENUATION_BIAS",0x15B:"LIGHT1_ATTENUATION_SCALE",0x15C:"015C",0x15D:"015D",
    0x15E:"015E",0x15F:"015F",0x160:"LIGHT2_SPECULAR0",0x161:"LIGHT2_SPECULAR1",
    0x162:"LIGHT2_DIFFUSE",0x163:"LIGHT2_AMBIENT",0x164:"LIGHT2_XY",0x165:"LIGHT2_Z",
    0x166:"LIGHT2_SPOTDIR_XY",0x167:"LIGHT2_SPOTDIR_Z",0x168:"0168",0x169:"LIGHT2_CONFIG",
    0x16A:"LIGHT2_ATTENUATION_BIAS",0x16B:"LIGHT2_ATTENUATION_SCALE",0x16C:"016C",0x16D:"016D",
    0x16E:"016E",0x16F:"016F",0x170:"LIGHT3_SPECULAR0",0x171:"LIGHT3_SPECULAR1",
    0x172:"LIGHT3_DIFFUSE",0x173:"LIGHT3_AMBIENT",0x174:"LIGHT3_XY",0x175:"LIGHT3_Z",
    0x176:"LIGHT3_SPOTDIR_XY",0x177:"LIGHT3_SPOTDIR_Z",0x178:"0178",0x179:"LIGHT3_CONFIG",
    0x17A:"LIGHT3_ATTENUATION_BIAS",0x17B:"LIGHT3_ATTENUATION_SCALE",0x17C:"017C",0x17D:"017D",
    0x17E:"017E",0x17F:"017F",0x180:"LIGHT4_SPECULAR0",0x181:"LIGHT4_SPECULAR1",
    0x182:"LIGHT4_DIFFUSE",0x183:"LIGHT4_AMBIENT",0x184:"LIGHT4_XY",0x185:"LIGHT4_Z",
    0x186:"LIGHT4_SPOTDIR_XY",0x187:"LIGHT4_SPOTDIR_Z",0x188:"0188",0x189:"LIGHT4_CONFIG",
    0x18A:"LIGHT4_ATTENUATION_BIAS",0x18B:"LIGHT4_ATTENUATION_SCALE",0x18C:"018C",0x18D:"018D",
    0x18E:"018E",0x18F:"018F",0x190:"LIGHT5_SPECULAR0",0x191:"LIGHT5_SPECULAR1",
    0x192:"LIGHT5_DIFFUSE",0x193:"LIGHT5_AMBIENT",0x194:"LIGHT5_XY",0x195:"LIGHT5_Z",
    0x196:"LIGHT5_SPOTDIR_XY",0x197:"LIGHT5_SPOTDIR_Z",0x198:"0198",0x199:"LIGHT5_CONFIG",
    0x19A:"LIGHT5_ATTENUATION_BIAS",0x19B:"LIGHT5_ATTENUATION_SCALE",0x19C:"019C",0x19D:"019D",
    0x19E:"019E",0x19F:"019F",0x1A0:"LIGHT6_SPECULAR0",0x1A1:"LIGHT6_SPECULAR1",
    0x1A2:"LIGHT6_DIFFUSE",0x1A3:"LIGHT6_AMBIENT",0x1A4:"LIGHT6_XY",0x1A5:"LIGHT6_Z",
    0x1A6:"LIGHT6_SPOTDIR_XY",0x1A7:"LIGHT6_SPOTDIR_Z",0x1A8:"01A8",0x1A9:"LIGHT6_CONFIG",
    0x1AA:"LIGHT6_ATTENUATION_BIAS",0x1AB:"LIGHT6_ATTENUATION_SCALE",0x1AC:"01AC",0x1AD:"01AD",
    0x1AE:"01AE",0x1AF:"01AF",0x1B0:"LIGHT7_SPECULAR0",0x1B1:"LIGHT7_SPECULAR1",
    0x1B2:"LIGHT7_DIFFUSE",0x1B3:"LIGHT7_AMBIENT",0x1B4:"LIGHT7_XY",0x1B5:"LIGHT7_Z",
    0x1B6:"LIGHT7_SPOTDIR_XY",0x1B7:"LIGHT7_SPOTDIR_Z",0x1B8:"01B8",0x1B9:"LIGHT7_CONFIG",
    0x1BA:"LIGHT7_ATTENUATION_BIAS",0x1BB:"LIGHT7_ATTENUATION_SCALE",0x1BC:"01BC",0x1BD:"01BD",
    0x1BE:"01BE",0x1BF:"01BF",0x1C0:"LIGHTING_AMBIENT",0x1C1:"01C1",0x1C2:"LIGHTING_NUM_LIGHTS",
    0x1C3:"LIGHTING_CONFIG0",0x1C4:"LIGHTING_CONFIG1",0x1C5:"LIGHTING_LUT_INDEX",
    0x1C6:"LIGHTING_ENABLE1",0x1C7:"01C7",0x1C8:"LIGHTING_LUT_DATA0",0x1C9:"LIGHTING_LUT_DATA1",
    0x1CA:"LIGHTING_LUT_DATA2",0x1CB:"LIGHTING_LUT_DATA3",0x1CC:"LIGHTING_LUT_DATA4",
    0x1CD:"LIGHTING_LUT_DATA5",0x1CE:"LIGHTING_LUT_DATA6",0x1CF:"LIGHTING_LUT_DATA7",
    0x1D0:"LIGHTING_LUTINPUT_ABS",0x1D1:"LIGHTING_LUTINPUT_SELECT",0x1D2:"LIGHTING_LUTINPUT_SCALE",
    0x1D3:"01D3",0x1D4:"01D4",0x1D5:"01D5",0x1D6:"01D6",0x1D7:"01D7",0x1D8:"01D8",
    0x1D9:"LIGHTING_LIGHT_PERMUTATION",0x1DA:"01DA",0x1DB:"01DB",0x1DC:"01DC",0x1DD:"01DD",
    0x1DE:"01DE",0x1DF:"01DF",0x1E0:"01E0",0x1E1:"01E1",0x1E2:"01E2",0x1E3:"01E3",0x1E4:"01E4",
    0x1E5:"01E5",0x1E6:"01E6",0x1E7:"01E7",0x1E8:"01E8",0x1E9:"01E9",0x1EA:"01EA",0x1EB:"01EB",
    0x1EC:"01EC",0x1ED:"01ED",0x1EE:"01EE",0x1EF:"01EF",0x1F0:"01F0",0x1F1:"01F1",0x1F2:"01F2",
    0x1F3:"01F3",0x1F4:"01F4",0x1F5:"01F5",0x1F6:"01F6",0x1F7:"01F7",0x1F8:"01F8",0x1F9:"01F9",
    0x1FA:"01FA",0x1FB:"01FB",0x1FC:"01FC",0x1FD:"01FD",0x1FE:"01FE",0x1FF:"01FF",
    0x200:"ATTRIBBUFFERS_LOC",0x201:"ATTRIBBUFFERS_FORMAT_LOW",0x202:"ATTRIBBUFFERS_FORMAT_HIGH",
    0x203:"ATTRIBBUFFER0_OFFSET",0x204:"ATTRIBBUFFER0_CONFIG1",0x205:"ATTRIBBUFFER0_CONFIG2",
    0x206:"ATTRIBBUFFER1_OFFSET",0x207:"ATTRIBBUFFER1_CONFIG1",0x208:"ATTRIBBUFFER1_CONFIG2",
    0x209:"ATTRIBBUFFER2_OFFSET",0x20A:"ATTRIBBUFFER2_CONFIG1",0x20B:"ATTRIBBUFFER2_CONFIG2",
    0x20C:"ATTRIBBUFFER3_OFFSET",0x20D:"ATTRIBBUFFER3_CONFIG1",0x20E:"ATTRIBBUFFER3_CONFIG2",
    0x20F:"ATTRIBBUFFER4_OFFSET",0x210:"ATTRIBBUFFER4_CONFIG1",0x211:"ATTRIBBUFFER4_CONFIG2",
    0x212:"ATTRIBBUFFER5_OFFSET",0x213:"ATTRIBBUFFER5_CONFIG1",0x214:"ATTRIBBUFFER5_CONFIG2",
    0x215:"ATTRIBBUFFER6_OFFSET",0x216:"ATTRIBBUFFER6_CONFIG1",0x217:"ATTRIBBUFFER6_CONFIG2",
    0x218:"ATTRIBBUFFER7_OFFSET",0x219:"ATTRIBBUFFER7_CONFIG1",0x21A:"ATTRIBBUFFER7_CONFIG2",
    0x21B:"ATTRIBBUFFER8_OFFSET",0x21C:"ATTRIBBUFFER8_CONFIG1",0x21D:"ATTRIBBUFFER8_CONFIG2",
    0x21E:"ATTRIBBUFFER9_OFFSET",0x21F:"ATTRIBBUFFER9_CONFIG1",0x220:"ATTRIBBUFFER9_CONFIG2",
    0x221:"ATTRIBBUFFERA_OFFSET",0x222:"ATTRIBBUFFERA_CONFIG1",0x223:"ATTRIBBUFFERA_CONFIG2",
    0x224:"ATTRIBBUFFERB_OFFSET",0x225:"ATTRIBBUFFERB_CONFIG1",0x226:"ATTRIBBUFFERB_CONFIG2",
    0x227:"INDEXBUFFER_CONFIG",0x228:"NUMVERTICES",0x229:"GEOSTAGE_CONFIG",0x22A:"VERTEX_OFFSET",
    0x22B:"022B",0x22C:"022C",0x22D:"POST_VERTEX_CACHE_NUM",0x22E:"DRAWARRAYS",0x22F:"DRAWELEMENTS",
    0x230:"0230",0x231:"VTX_FUNC",0x232:"FIXEDATTRIB_INDEX",0x233:"FIXEDATTRIB_DATA0",
    0x234:"FIXEDATTRIB_DATA1",0x235:"FIXEDATTRIB_DATA2",0x236:"0236",0x237:"0237",
    0x238:"CMDBUF_SIZE0",0x239:"CMDBUF_SIZE1",0x23A:"CMDBUF_ADDR0",0x23B:"CMDBUF_ADDR1",
    0x23C:"CMDBUF_JUMP0",0x23D:"CMDBUF_JUMP1",0x23E:"023E",0x23F:"023F",0x240:"0240",0x241:"0241",
    0x242:"VSH_NUM_ATTR",0x243:"0243",0x244:"VSH_COM_MODE",0x245:"START_DRAW_FUNC0",0x246:"0246",
    0x247:"0247",0x248:"0248",0x249:"0249",0x24A:"VSH_OUTMAP_TOTAL1",0x24B:"024B",0x24C:"024C",
    0x24D:"024D",0x24E:"024E",0x24F:"024F",0x250:"0250",0x251:"VSH_OUTMAP_TOTAL2",0x252:"GSH_MISC0",
    0x253:"GEOSTAGE_CONFIG2",0x254:"GSH_MISC1",0x255:"0255",0x256:"0256",0x257:"0257",0x258:"0258",
    0x259:"0259",0x25A:"025A",0x25B:"025B",0x25C:"025C",0x25D:"025D",0x25E:"PRIMITIVE_CONFIG",
    0x25F:"RESTART_PRIMITIVE",0x260:"0260",0x261:"0261",0x262:"0262",0x263:"0263",0x264:"0264",
    0x265:"0265",0x266:"0266",0x267:"0267",0x268:"0268",0x269:"0269",0x26A:"026A",0x26B:"026B",
    0x26C:"026C",0x26D:"026D",0x26E:"026E",0x26F:"026F",0x270:"0270",0x271:"0271",0x272:"0272",
    0x273:"0273",0x274:"0274",0x275:"0275",0x276:"0276",0x277:"0277",0x278:"0278",0x279:"0279",
    0x27A:"027A",0x27B:"027B",0x27C:"027C",0x27D:"027D",0x27E:"027E",0x27F:"027F",
    0x280:"GSH_BOOLUNIFORM",0x281:"GSH_INTUNIFORM_I0",0x282:"GSH_INTUNIFORM_I1",
    0x283:"GSH_INTUNIFORM_I2",0x284:"GSH_INTUNIFORM_I3",0x285:"0285",0x286:"0286",0x287:"0287",
    0x288:"0288",0x289:"GSH_INPUTBUFFER_CONFIG",0x28A:"GSH_ENTRYPOINT",
    0x28B:"GSH_ATTRIBUTES_PERMUTATION_LOW",0x28C:"GSH_ATTRIBUTES_PERMUTATION_HIGH",
    0x28D:"GSH_OUTMAP_MASK",0x28E:"028E",0x28F:"GSH_CODETRANSFER_END",
    0x290:"GSH_FLOATUNIFORM_CONFIG",0x291:"GSH_FLOATUNIFORM_DATA",0x299:"0299",0x29A:"029A",
    0x29B:"GSH_CODETRANSFER_CONFIG",0x29C:"GSH_CODETRANSFER_DATA",0x2A4:"02A4",
    0x2A5:"GSH_OPDESCS_CONFIG",0x2A6:"GSH_OPDESCS_DATA",0x2AE:"02AE",0x2AF:"02AF",
    0x2B0:"VSH_BOOLUNIFORM",0x2B1:"VSH_INTUNIFORM_I0",0x2B2:"VSH_INTUNIFORM_I1",
    0x2B3:"VSH_INTUNIFORM_I2",0x2B4:"VSH_INTUNIFORM_I3",0x2B5:"02B5",0x2B6:"02B6",0x2B7:"02B7",
    0x2B8:"02B8",0x2B9:"VSH_INPUTBUFFER_CONFIG",0x2BA:"VSH_ENTRYPOINT",
    0x2BB:"VSH_ATTRIBUTES_PERMUTATION_LOW",0x2BC:"VSH_ATTRIBUTES_PERMUTATION_HIGH",
    0x2BD:"VSH_OUTMAP_MASK",0x2BE:"02BE",0x2BF:"VSH_CODETRANSFER_END",
    0x2C0:"VSH_FLOATUNIFORM_CONFIG",0x2C1:"VSH_FLOATUNIFORM_DATA",0x2C9:"02C9",0x2CA:"02CA",
    0x2CB:"VSH_CODETRANSFER_CONFIG",0x2CC:"VSH_CODETRANSFER_DATA",0x2D4:"02D4",
    0x2D5:"VSH_OPDESCS_CONFIG",0x2D6:"VSH_OPDESCS_DATA",0x2DE:"02DE",0x2DF:"02DF",0x2E0:"02E0",
    0x2E1:"02E1",0x2E2:"02E2",0x2E3:"02E3",0x2E4:"02E4",0x2E5:"02E5",0x2E6:"02E6",0x2E7:"02E7",
    0x2E8:"02E8",0x2E9:"02E9",0x2EA:"02EA",0x2EB:"02EB",0x2EC:"02EC",0x2ED:"02ED",0x2EE:"02EE",
    0x2EF:"02EF",0x2F0:"02F0",0x2F1:"02F1",0x2F2:"02F2",0x2F3:"02F3",0x2F4:"02F4",0x2F5:"02F5",
    0x2F6:"02F6",0x2F7:"02F7",0x2F8:"02F8",0x2F9:"02F9",0x2FA:"02FA",0x2FB:"02FB",0x2FC:"02FC",
    0x2FD:"02FD",0x2FE:"02FE",0x2FF:"02FF",
}


# ---------------------------------------------------------------- encoder
def encode_command(reg, params, mask=0xF, incremental=False):
    """Mirror of libctru GPUCMD_Add: the words for one logical write."""
    out = []
    params = list(params) or [0]
    while params:
        chunk, params = params[:0x100], params[0x100:]
        extra = len(chunk) - 1
        header = (int(incremental) << 31) | ((mask & 0xF) << 16) | (reg & 0x3FF) | (extra << 20)
        out += [chunk[0], header] + chunk[1:]
        if extra & 1:
            out.append(0)
        if incremental:
            reg += len(chunk)
    return out


def split_finalize(words):
    """Mirror of GPUCMD_Split: FINALIZE, plus a second one if not 16-byte aligned."""
    words = list(words) + encode_command(REG_FINALIZE, [0x12345678])
    if len(words) & 3:
        words += encode_command(REG_FINALIZE, [0x12345678])
    return words


# ---------------------------------------------------------------- decoder
class Cmd:
    def __init__(self, index, word_off, reg, mask, incr, params):
        self.index, self.word_off, self.reg = index, word_off, reg
        self.mask, self.incr, self.params = mask, incr, params

    def writes(self):
        for k, p in enumerate(self.params):
            yield self.reg + (k if self.incr else 0), p


def decode(words):
    """Return (commands, warnings). Never raises on malformed input."""
    cmds, warns = [], []
    i, n = 0, len(words)
    if n & 3:
        warns.append(f"list is {n} words; GPUCMD_Split pads to a multiple of 4 words (16 bytes)")
    while i < n:
        if i + 1 >= n:
            warns.append(f"word {i}: truncated command (parameter word without header)")
            break
        param0, header = words[i], words[i + 1]
        reg = header & 0x3FF
        mask = (header >> 16) & 0xF
        extra = (header >> 20) & 0xFF
        incr = bool(header >> 31)
        if header & 0xFC00:
            warns.append(f"word {i + 1}: header bits 10-15 set (0x{header & 0xFC00:04X}); libctru masks the id to 10 bits")
        if (header >> 28) & 7:
            warns.append(f"word {i + 1}: header bits 28-30 set (documented as unused)")
        total = 2 + extra
        start = i
        if i + total > n:
            warns.append(f"word {i}: command wants {total} words, only {n - i} left (truncated)")
            cmds.append(Cmd(len(cmds), i, reg, mask, incr, [param0] + words[i + 2:n]))
            break
        cmds.append(Cmd(len(cmds), i, reg, mask, incr, [param0] + words[i + 2:i + total]))
        i += total
        if total & 1:
            if i < n:
                if words[i] != 0:
                    warns.append(f"word {i}: alignment pad word is 0x{words[i]:08X}, libctru writes 0")
                i += 1
            else:
                warns.append(f"word {i}: missing alignment pad word after an odd-length command")
        if mask == 0:
            warns.append(f"word {start}: write mask 0 (nothing is written)")
        for r, _ in cmds[-1].writes():
            if r > 0x2FF or r not in REG_NAMES:
                warns.append(f"word {start}: register 0x{r:03X} is not a known register name")
                break
    fins = [c for c in cmds if c.reg == REG_FINALIZE]
    if not fins:
        warns.append("no GPUREG_FINALIZE: the end-of-list (P3D) interrupt would never fire (3dbrew)")
    else:
        for c in cmds[fins[0].index + 1:]:
            if c.reg != REG_FINALIZE:
                warns.append(f"word {c.word_off}: command after FINALIZE; 3dbrew says the GPU ignores it")
                break
    return cmds, warns


def stride_check(state, buf):
    """Compare ATTRIBBUFFER<buf> stride with the size implied by FORMAT + CONFIG1/2.

    Reads the format fields as libctru does (type = bits 0-1, count-1 = bits 2-3) and
    applies the element-size alignment rule from 3dbrew GPU/Pitfalls. A hint only.
    """
    lo, hi = state.get(REG_FORMAT_LOW), state.get(REG_FORMAT_HIGH)
    c1, c2 = state.get(0x204 + 3 * buf), state.get(0x205 + 3 * buf)
    if None in (lo, hi, c1, c2) or not (c2 >> 28):
        return None
    fmt = lo | ((hi & 0xFFFF) << 32)
    comps = [(c1 >> (4 * k)) & 0xF for k in range(8)] + [(c2 >> (4 * k)) & 0xF for k in range(4)]
    ncomp, stride = c2 >> 28, (c2 >> 16) & 0xFF
    off = 0
    for cid in comps[:ncomp]:
        if cid >= 12:                      # padding attribute: 4, 8, 12 or 16 bytes
            off += 4 * (cid - 11)
            continue
        f = (fmt >> (4 * cid)) & 0xF
        esize = (1, 1, 2, 4)[f & 3]
        off = (off + esize - 1) & ~(esize - 1)
        off += esize * ((f >> 2) + 1)
    if off != stride:
        return (f"buffer {buf}: stride field is {stride} but format/permutation imply {off} bytes "
                "(3dbrew Pitfalls: a mismatch can freeze the GPU or draw nothing)")
    return None


def report(words, out=sys.stdout):
    cmds, warns = decode(words)
    state, draws = {}, 0
    for c in cmds:
        nm = REG_NAMES.get(c.reg, f"0x{c.reg:03X}")
        out.write(f"[{c.index:3d}] @w{c.word_off:<5d} {nm:<30s} mask={c.mask:X} {'incr' if c.incr else 'same'} n={len(c.params)}\n")
        for r, p in c.writes():
            out.write(f"        {REG_NAMES.get(r, '0x%03X' % r):<34s} <- 0x{p:08X}\n")
            if c.mask == 0xF:
                state[r] = p
            if r in (REG_DRAWARRAYS, REG_DRAWELEMENTS) and p:
                draws += 1
                out.write(f"        ^ draw #{draws}: NUMVERTICES={state.get(REG_NUMVERTICES)}\n")
                if REG_ATTRIBBUFFERS_LOC not in state:
                    warns.append(f"draw #{draws}: ATTRIBBUFFERS_LOC not written in this list (fine only if an earlier list set it)")
                for b in range(12):
                    w = stride_check(state, b)
                    if w:
                        warns.append(f"draw #{draws}: {w}")
    out.write(f"\n{len(words)} words, {len(cmds)} commands, {draws} draw triggers\n")
    for w in warns:
        out.write(f"WARNING: {w}\n")
    return cmds, warns


# ---------------------------------------------------------------- self-test
def _draw_arrays_list(stride):
    """Synthetic list shaped like C3D_DrawArrays output (built by hand from citro3d drawArrays.c)."""
    lst = encode_command(REG_ATTRIBBUFFERS_LOC, [0x18000000 >> 3])
    # attr0 = 3 floats (count-1=2, type 3 -> 0xB), attr1 = 2 floats (0x7); 2 attributes
    lst += encode_command(REG_FORMAT_LOW, [0xB | (0x7 << 4), 1 << 28], incremental=True)
    lst += encode_command(0x203, [0, 0x10, (2 << 28) | (stride << 16)], incremental=True)
    lst += encode_command(0x25E, [0], mask=2)
    lst += encode_command(0x25F, [1])
    lst += encode_command(0x227, [0x80000000])
    lst += encode_command(REG_NUMVERTICES, [3])
    lst += encode_command(0x22A, [0])
    lst += encode_command(0x253, [1], mask=1)
    lst += encode_command(0x245, [0], mask=1)
    lst += encode_command(REG_DRAWARRAYS, [1])
    lst += encode_command(0x245, [1], mask=1)
    lst += encode_command(0x253, [0], mask=1)
    lst += encode_command(0x231, [1])
    return split_finalize(lst)


def _selftest():
    ok = True

    def check(name, cond):
        nonlocal ok
        print(("PASS " if cond else "FAIL ") + name)
        ok = ok and bool(cond)

    check("single write layout", encode_command(0x228, [3]) == [3, 0x000F0228])
    check("masked write layout", encode_command(0x25E, [0x100], mask=2) == [0x100, 0x0002025E])
    check("3 incremental params: extra=2, no pad",
          encode_command(0x203, [1, 2, 3], incremental=True) == [1, 0x802F0203, 2, 3])
    w = encode_command(0x203, [1, 2], incremental=True)
    check("2 incremental params: extra=1, pad word", w == [1, 0x801F0203, 2, 0])
    w = encode_command(0x2C1, list(range(0x101)), incremental=True)
    c, _ = decode(w)
    check("0x101 params split 0x100 + 1, register id advances",
          len(c) == 2 and len(c[0].params) == 0x100 and c[1].reg == 0x2C1 + 0x100)

    good = _draw_arrays_list(20)
    cmds, warns = decode(good)
    check("synthetic DrawArrays list is 16-byte aligned", len(good) % 4 == 0)
    check("synthetic DrawArrays list: no warnings", warns == [])
    _, w2 = report(good, io.StringIO())
    check("stride 20 = 3 floats + 2 floats accepted", not any("stride" in x for x in w2))

    _, w3 = decode(encode_command(REG_NUMVERTICES, [3]) + encode_command(REG_DRAWARRAYS, [1]))
    check("missing FINALIZE flagged", any("FINALIZE" in x for x in w3))
    _, w4 = report(_draw_arrays_list(24), io.StringIO())
    check("wrong stride 24 flagged", any("stride field is 24" in x for x in w4))
    _, w5 = decode(good[:-3])
    check("truncated list flagged", any("truncated" in x or "multiple of 4" in x for x in w5))
    after = split_finalize(encode_command(0x228, [1])) + encode_command(0x228, [2])
    _, w6 = decode(after)
    check("command after FINALIZE flagged", any("after FINALIZE" in x for x in w6))
    bad_pad = encode_command(0x203, [1, 2], incremental=True)
    bad_pad[3] = 0xDEAD
    _, w7 = decode(bad_pad)
    check("non-zero pad word flagged", any("pad word" in x for x in w7))
    return ok


def main(argv):
    if len(argv) >= 2 and argv[1] == "--selftest":
        return 0 if _selftest() else 1
    if len(argv) >= 3 and argv[1] == "--hex":
        with open(argv[2]) as f:
            words = [int(t, 16) for t in f.read().split()]
    elif len(argv) == 2:
        with open(argv[1], "rb") as f:
            data = f.read()
        words = list(struct.unpack("<%dI" % (len(data) // 4), data[: len(data) // 4 * 4]))
    else:
        print(__doc__)
        return 2
    _, warns = report(words)
    return 1 if warns else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
