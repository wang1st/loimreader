/* RFC 8032 test key; not a production signing key. */
#define LOIM_ENTITLEMENT_KEY_ID "loim-20261016"
static const uint8_t loim_entitlement_public_key[32] = {0xd7,0x5a,0x98,0x1,0x82,0xb1,0xa,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x7,0x3a,0xe,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x2,0x1a,0x68,0xf7,0x7,0x51,0x1a};
#if defined(LOIM_ENTITLEMENT_VECTOR_DATA)
static const char *const test_payloads[] = {
"LoimReader-License-1\nkey=loim-20261016\naccount=70616964406578616d706c652e636f6d\ndevice=9001-5098-3CD2-4FB0\nnonce=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\nissued=1700000000\nexpires=1700604800\nlicensed=1\n",
"LoimReader-License-1\nkey=loim-20261016\naccount=70616964406578616d706c652e636f6d\ndevice=9001-5098-3CD2-4FB0\nnonce=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\nissued=1700000000\nexpires=1700604800\nlicensed=0\n",
"LoimReader-License-1\nkey=wrong-key-id\naccount=70616964406578616d706c652e636f6d\ndevice=9001-5098-3CD2-4FB0\nnonce=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\nissued=1700000000\nexpires=1700604800\nlicensed=1\n",
"LoimReader-License-1\nkey=loim-20261016\naccount=70616964406578616d706c652e636f6d\ndevice=9001-5098-3CD2-4FB0\nnonce=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\nissued=1700000000\nexpires=1700604801\nlicensed=1\n",
"LoimReader-License-1\nkey=loim-20261016\naccount=70616964406578616d706c652e636f6d\ndevice=9001-5098-3CD2-4FB0\nnonce=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\nissued=1700000000\nexpires=1700000000\nlicensed=1\n",
"LoimReader-License-1\nkey=loim-20261016\naccount=70616964406578616d706c652e636f6d\ndevice=9001-5098-3CD2-4FB0\nnonce=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\nissued=1700000000\nexpires=18446744073709551616\nlicensed=1\n",
"LoimReader-License-1\nkey=loim-20261016\naccount=70616964406578616d706c652e636f6d\ndevice=9001-5098-3CD2-4FB0\nnonce=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\nissued=1700000000\nexpires=1700604800\nlicensed=1\nextra=1\n"
};
static const char *const test_signatures[] = {
"cc99492247639bddcdc0739d1ce14a3d9d887acab6f0b25b2f7e757f419555e00e22e5f07a4062bd6fd98067b4f8d1a234f8ce7581dd64b98a36f41c787bae0c",
"533e67a48e4b2329c177f58ed0769137189dd84fee9a4ff3e74cc71069ae7bb79817e07c877dd7887db9b29312d9382c22c94eab63206dcf977ae185497fe70b",
"786dfbea8fb8cd82bbe19c2df996f5febcab69524a0ea06e3b0447889d1a362533f1898511ee7a7825836f384bd0c6f3934bec384aff203fa58d0faf0297560d",
"49160bb1c02d9bff7b02ba787349ba4ec56983a5707f7acd4ee0b9a59817851a3a5cfdf99b3018544b2188c5d8cce9d3b234bd4ab893cf9f96488a598978ed08",
"c764db94b42871801845b561ecbc8fb68213fb2fa70f00931889144aebb50330ce6524d0f23a2c37b9fd9d046403479629dd194d8f35e6e689ae4decde83b908",
"11feada03ede32f94d28752698ed222d1c0dcf0e30e3f0dc182ea9056e0567035eb92413eaa1d6f83421550d645c17626202345997f5e5469b3724a1a53e6604",
"a5bdb1f482ada5546bc200b13033c13d6cc6f3cbe1511e45ebad6fab2f9bafe79405a56bd4ccc9112f42b3aced933fe8628d4259be8ff3e4b03a8ed6a3b52e01"
};

#endif
