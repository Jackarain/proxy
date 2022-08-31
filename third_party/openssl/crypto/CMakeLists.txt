if(WIN32)
ENABLE_LANGUAGE(ASM_MASM)
else()
ENABLE_LANGUAGE(ASM)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include/ ../include)

include_directories(asn1)
include_directories(evp)
include_directories(modes)

set(GCM_SRC modes/cbc128.c  modes/ccm128.c  modes/cfb128.c  modes/ctr128.c  modes/gcm128.c  modes/ocb128.c  modes/ofb128.c  modes/wrap128.c  modes/xts128.c)

if (WIN32)
	set(ASYNC_ARCH async/arch/async_win.c async/arch/async_null.c)
elseif (APPLE)
	set(ASYNC_ARCH async/arch/async_null.c)
else()
	set(ASYNC_ARCH async/arch/async_posix.c async/arch/async_null.c)
endif()

file(GLOB SRCS
	aes/*.c

	aria/*.c
	asn1/asn1_item_list.c asn1/a_object.c  asn1/a_bitstr.c  asn1/a_utctm.c  asn1/a_gentm.c  asn1/a_time.c  asn1/a_int.c  asn1/a_octet.c  asn1/a_print.c  asn1/a_type.c  asn1/a_dup.c  asn1/a_d2i_fp.c  asn1/a_i2d_fp.c  asn1/a_utf8.c  asn1/a_sign.c  asn1/a_digest.c  asn1/a_verify.c  asn1/a_mbstr.c  asn1/a_strex.c  asn1/x_algor.c  asn1/x_val.c  asn1/x_sig.c  asn1/x_bignum.c  asn1/x_long.c  asn1/x_int64.c  asn1/x_info.c  asn1/x_spki.c  asn1/nsseq.c  asn1/d2i_pu.c  asn1/d2i_pr.c  asn1/i2d_pu.c  asn1/i2d_pr.c  asn1/t_pkey.c  asn1/t_spki.c  asn1/t_bitst.c  asn1/tasn_new.c  asn1/tasn_fre.c  asn1/tasn_enc.c  asn1/tasn_dec.c  asn1/tasn_utl.c  asn1/tasn_typ.c  asn1/tasn_prn.c  asn1/tasn_scn.c  asn1/ameth_lib.c  asn1/f_int.c  asn1/f_string.c  asn1/n_pkey.c  asn1/x_pkey.c  asn1/bio_asn1.c  asn1/bio_ndef.c  asn1/asn_mime.c  asn1/asn1_gen.c  asn1/asn1_par.c  asn1/asn1_lib.c  asn1/asn1_err.c  asn1/a_strnid.c  asn1/evp_asn1.c  asn1/asn_pack.c  asn1/p5_pbe.c  asn1/p5_pbev2.c  asn1/p5_scrypt.c  asn1/p8_pkey.c  asn1/asn_moid.c  asn1/asn_mstbl.c

	async/*

	bf/bf_skey.c bf/bf_ecb.c bf/bf_cfb64.c bf/bf_ofb64.c bf/bf_enc.c

	bio/*.c
	blake2/*.c
	bn/*.c
	buffer/*.c
	camellia/*.c
	cast/*.c
	cmac/*.c
	cms/*.c
	conf/*.c
	ct/*.c

	dh/*.c

	des/cbc_cksm.c
	des/cbc_enc.c
	des/cfb64ede.c
	des/cfb64enc.c
	des/cfb_enc.c
	des/des_enc.c
	des/ecb3_enc.c
	des/ecb_enc.c
	des/fcrypt_b.c
	des/fcrypt.c
	des/ofb64ede.c
	des/ofb64enc.c
	des/ofb_enc.c
	des/pcbc_enc.c
	des/qud_cksm.c
	des/rand_key.c
	des/set_key.c
	des/str2key.c
	des/xcbc_enc.c

	dsa/*.c
	dso/*.c

	ec/ec_lib.c
	ec/ecp_smpl.c
	ec/ecp_mont.c
	ec/ecp_nist.c
	ec/ec_cvt.c
	ec/ec_mult.c
	ec/ec_err.c
	ec/ec_curve.c
	ec/ec_check.c
	ec/ec_print.c
	ec/ec_asn1.c
	ec/ec_key.c
	ec/ec2_smpl.c
	ec/ec_ameth.c
	ec/ec_pmeth.c
	ec/eck_prn.c
	ec/ecp_nistp224.c
	ec/ecp_nistp256.c
	ec/ecp_nistp521.c
	ec/ecp_nistputil.c
	ec/ecp_oct.c
	ec/ec2_oct.c
	ec/ec_oct.c
	ec/ec_kmeth.c
	ec/ecdh_ossl.c
	ec/ecdh_kdf.c
	ec/ecdsa_ossl.c
	ec/ecdsa_sign.c
	ec/ecdsa_vrf.c
	ec/curve25519.c
	ec/ecx_meth.c
	ec/curve448/arch_32/f_impl.c ec/curve448/f_generic.c ec/curve448/scalar.c
	ec/curve448/curve448_tables.c ec/curve448/eddsa.c ec/curve448/curve448.c
	ecdsa/*.c
	ecdh/*.c
	err/*.c
	evp/*.c

	hmac/*.c
	idea/*.c
	kdf/*.c
	lhash/*.c
	md5/*.c


	objects/*.c
	ocsp/*.c
	pem/*.c
	pqueue/*.c

	pkcs7/pk7_asn1.c pkcs7/pk7_lib.c pkcs7/pkcs7err.c pkcs7/pk7_doit.c pkcs7/pk7_smime.c pkcs7/pk7_attr.c pkcs7/pk7_mime.c pkcs7/bio_pk7.c

	pkcs12/*.c

	poly1305/poly1305_ameth.c poly1305/poly1305_pmeth.c poly1305/poly1305.c

	rand/*.c
	rc2/*.c
	rc4/*.c
	rc5/*.c
	rsa/*.c
	ripemd/*.c

	seed/*.c
	sha/*.c
	siphash/siphash_ameth.c  siphash/siphash.c  siphash/siphash_pmeth.c
	sm2/*.c
	sm3/*.c
	sm4/*.c
	srp/*.c
	stack/*.c
	store/*.c
	ts/*.c
	txt_db/*.c
	ui/*.c
	x509/*.c

	x509v3/v3_admis.c x509v3/v3_bcons.c  x509v3/v3_bitst.c  x509v3/v3_conf.c  x509v3/v3_extku.c  x509v3/v3_ia5.c  x509v3/v3_lib.c  x509v3/v3_prn.c  x509v3/v3_utl.c  x509v3/v3err.c  x509v3/v3_genn.c  x509v3/v3_alt.c  x509v3/v3_skey.c  x509v3/v3_akey.c  x509v3/v3_pku.c  x509v3/v3_int.c  x509v3/v3_enum.c  x509v3/v3_sxnet.c  x509v3/v3_cpols.c  x509v3/v3_crld.c  x509v3/v3_purp.c  x509v3/v3_info.c  x509v3/v3_akeya.c  x509v3/v3_pmaps.c  x509v3/v3_pcons.c  x509v3/v3_ncons.c  x509v3/v3_pcia.c  x509v3/v3_pci.c  x509v3/pcy_cache.c  x509v3/pcy_node.c  x509v3/pcy_data.c  x509v3/pcy_map.c  x509v3/pcy_tree.c  x509v3/pcy_lib.c  x509v3/v3_asid.c  x509v3/v3_addr.c  x509v3/v3_tlsf.c

	ctype.c
	cversion.c  ex_data.c    mem.c      mem_sec.c  o_fopen.c  o_time.c    threads_win.c
	cpt_err.c   dllmain.c   getenv.c     mem_clr.c  o_dir.c    o_init.c   threads_none.c     uid.c
	cryptlib.c  ebcdic.c    init.c       mem_dbg.c  o_fips.c   o_str.c    threads_pthread.c
)

if (SHARED_OPENSSL)
add_library(crypto SHARED ${SRCS} ${ASYNC_ARCH} ${GCM_SRC})
else()
add_library(crypto STATIC ${SRCS} ${ASYNC_ARCH} ${GCM_SRC})
endif()

set_target_properties(crypto PROPERTIES FOLDER "third_party/openssl")

target_compile_definitions(crypto PUBLIC
	-DOPENSSL_NO_ASM
	-DOPENSSL_NO_ASYNC
	-DOPENSSL_NO_ENGINE
	-DOPENSSL_NO_RC1
	-DOPENSSL_NO_RC3
	-DOPENSSL_NO_MD1
	-DOPENSSL_NO_MD2
	-DOPENSSL_NO_MD3
	-DOPENSSL_NO_MD4
	-DOPENSSL_NO_MDC2
	-DOPENSSL_NO_WHIRLPOOL
	-DOPENSSL_NO_COMP
	-DOPENSSL_NO_CHACHA
	-DOPENSSLDIR=""
	-DWIN32_LEAN_AND_MEAN )

target_include_directories(crypto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include/)


if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
	if (MSVC)
		target_compile_definitions(crypto PUBLIC -DSIXTY_FOUR_BIT)
	else()
		target_compile_definitions(crypto PUBLIC -DSIXTY_FOUR_BIT_LONG)
	endif()
else()
	target_compile_definitions(crypto PUBLIC -DTHIRTY_TWO_BIT)
endif()

if (APPLE)
	target_compile_definitions(crypto PRIVATE -DASYNC_NULL)
endif()

if(NOT MSVC AND NOT APPLE)
	target_link_libraries(crypto PUBLIC)
endif()

if(MINGW)
	target_link_libraries(crypto PUBLIC ws2_32)
endif()

find_package(Threads)
target_link_libraries(crypto ${CMAKE_THREAD_LIBS_INIT})

