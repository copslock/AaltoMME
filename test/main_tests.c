/* AaltoMME - Mobility Management Entity for LTE networks
 * Copyright (C) 2013 Vicent Ferrer Guash & Jesus Llorente Santos
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file   main_tests.c
 * @Author Vicent Ferrer
 * @date   September, 2015
 * @brief  GLib based tests
 */

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <openssl/cmac.h>
#include "eia2.h"
#include "NAS.h"
#include "NASHandler.h"

static void test_kdf_test1(){
    g_assert (1 == 1);
}


void printBytes(unsigned char *buf, size_t len) {
    int i;
    for(i=0; i<len; i++) {
        printf("%02x ", buf[i]);
    }
    printf("\n");
}

static void test_cmac(){

    unsigned char key[] = { 0x2b,0x7e,0x15,0x16,
                            0x28,0xae,0xd2,0xa6,
                            0xab,0xf7,0x15,0x88,
                            0x09,0xcf,0x4f,0x3c};

  // M: 6bc1bee2 2e409f96 e93d7e11 7393172a Mlen: 128
    unsigned char message[] = { 0x6b,0xc1,0xbe,0xe2,
                                0x2e,0x40,0x9f,0x96,
                                0xe9,0x3d,0x7e,0x11,
                                0x73,0x93,0x17,0x2a };

    unsigned char mact[16] = {0};
    unsigned char mact_x[16] = { 0x07, 0x0a, 0x16, 0xb4,
                                 0x6b, 0x4d, 0x41, 0x44,
                                 0xf7, 0x9b, 0xdd, 0x9d,
                                 0xd0, 0x4a, 0x28, 0x7c	};
    size_t mactlen;

    CMAC_CTX *ctx = CMAC_CTX_new();
    CMAC_Init(ctx, key, 16, EVP_aes_128_cbc(), NULL);
    /* printf("message length = %lu bytes (%lu bits)\n",sizeof(message), sizeof(message)*8); */

    CMAC_Update(ctx, message, sizeof(message));
    CMAC_Final(ctx, mact, &mactlen);

    g_assert_true(memcmp (mact, mact_x, 4) == 0);

    /* printBytes(mact, mactlen); */
    /* expected result T = 070a16b4 6b4d4144 f79bdd9d d04a287c */

    CMAC_CTX_free(ctx);
}

/*
static void test_eia2_TestSet5(){
    const guint8 countI[] = {};
    const guint8 bearer = ;
    const guint8 direction = ;
    const guint8 ik[] = {};

    const gsize len = ;
    const guint8 msg[] = {};

    const guint8 mact_x[] = {};
    guint8 mact[4] = {0};
    eia2(ik, countI, bearer, direction, msg, len, mact);
    printf("mact %#x%x%x%x\n", mact[0], mact[1], mact[2], mact[3]);
    g_assert_true(memcmp (mact, mact_x, 4) == 0);
}
*/

static void test_eia2_TestSet6(){
    const guint8 countI[] = {0x36, 0xaf, 0x61, 0x44};
    const guint8 bearer = 0x18;
    const guint8 direction = 0x0;
    const guint8 ik[] = {0x68, 0x32, 0xa6, 0x5c, 0xff, 0x44, 0x73, 0x62,
                         0x1e, 0xbd, 0xd4, 0xba, 0x26, 0xa9, 0x21, 0xfe};

    const gsize len = 383;
    const guint8 msg[] = {0xd3, 0xc5, 0x38, 0x39, 0x62, 0x68, 0x20, 0x71,
                          0x77, 0x65, 0x66, 0x76, 0x20, 0x32, 0x38, 0x37,
                          0x63, 0x62, 0x40, 0x98, 0x1b, 0xa6, 0x82, 0x4c,
                          0x1b, 0xfb, 0x1a, 0xb4, 0x85, 0x47, 0x20, 0x29,
                          0xb7, 0x1d, 0x80, 0x8c, 0xe3, 0x3e, 0x2c, 0xc3,
                          0xc0, 0xb5, 0xfc, 0x1f, 0x3d, 0xe8, 0xa6, 0xdc};

    const guint8 mact_x[] = {0xf0, 0x66, 0x8c, 0x1e};
    guint8 mact[4] = {0};
    eia2(ik, countI, bearer, direction, msg, len, mact);
    /* printf("mact %#x%x%x%x\n", mact[0], mact[1], mact[2], mact[3]); */
    g_assert_true(memcmp (mact, mact_x, 4) == 0);
}

static void test_eia2_TestSet5(){
    const guint8 countI[] = {0x36, 0xaf, 0x61, 0x44};
    const guint8 bearer = 0x0f;
    const guint8 direction = 0x1;
    const guint8 ik[] = {0x83, 0xfd, 0x23, 0xa2, 0x44, 0xa7, 0x4c, 0xf3,
                         0x58, 0xda, 0x30, 0x19, 0xf1, 0x72, 0x26, 0x35};

    const gsize len = 768;
    const guint8 msg[] = {0x35, 0xc6, 0x87, 0x16, 0x63, 0x3c, 0x66, 0xfb,
                          0x75, 0x0c, 0x26, 0x68, 0x65, 0xd5, 0x3c, 0x11,
                          0xea, 0x05, 0xb1, 0xe9, 0xfa, 0x49, 0xc8, 0x39,
                          0x8d, 0x48, 0xe1, 0xef, 0xa5, 0x90, 0x9d, 0x39,
                          0x47, 0x90, 0x28, 0x37, 0xf5, 0xae, 0x96, 0xd5,
                          0xa0, 0x5b, 0xc8, 0xd6, 0x1c, 0xa8, 0xdb, 0xef,
                          0x1b, 0x13, 0xa4, 0xb4, 0xab, 0xfe, 0x4f, 0xb1,
                          0x00, 0x60, 0x45, 0xb6, 0x74, 0xbb, 0x54, 0x72,
                          0x93, 0x04, 0xc3, 0x82, 0xbe, 0x53, 0xa5, 0xaf,
                          0x05, 0x55, 0x61, 0x76, 0xf6, 0xea, 0xa2, 0xef,
                          0x1d, 0x05, 0xe4, 0xb0, 0x83, 0x18, 0x1e, 0xe6,
                          0x74, 0xcd, 0xa5, 0xa4, 0x85, 0xf7, 0x4d, 0x7a};

    const guint8 mact_x[] = {0xe6, 0x57, 0xe1, 0x82};
    guint8 mact[4] = {0};
    eia2(ik, countI, bearer, direction, msg, len, mact);
    /* printf("mact %#x%x%x%x\n", mact[0], mact[1], mact[2], mact[3]); */
    g_assert_true(memcmp (mact, mact_x, 4) == 0);
}

static void test_eia2_TestSet4(){
    const guint8 countI[] = {0xc7, 0x59, 0x0e, 0xa9};
    const guint8 bearer = 0x17;
    const guint8 direction = 0x0;
    const guint8 ik[] = {0xd3, 0x41, 0x9b, 0xe8, 0x21, 0x08, 0x7a, 0xcd,
                         0x02, 0x12, 0x3a, 0x92, 0x48, 0x03, 0x33, 0x59};

    const gsize len = 511;
    const guint8 msg[] = {0xbb, 0xb0, 0x57, 0x03, 0x88, 0x09, 0x49, 0x6b,
                          0xcf, 0xf8, 0x6d, 0x6f, 0xbc, 0x8c, 0xe5, 0xb1,
                          0x35, 0xa0, 0x6b, 0x16, 0x60, 0x54, 0xf2, 0xd5,
                          0x65, 0xbe, 0x8a, 0xce, 0x75, 0xdc, 0x85, 0x1e,
                          0x0b, 0xcd, 0xd8, 0xf0, 0x71, 0x41, 0xc4, 0x95,
                          0x87, 0x2f, 0xb5, 0xd8, 0xc0, 0xc6, 0x6a, 0x8b,
                          0x6d, 0xa5, 0x56, 0x66, 0x3e, 0x4e, 0x46, 0x12,
                          0x05, 0xd8, 0x45, 0x80, 0xbe, 0xe5, 0xbc, 0x7e};
    const guint8 mact_x[] = {0x68, 0x46, 0xa2, 0xf0};
    guint8 mact[4] = {0};
    eia2(ik, countI, bearer, direction, msg, len, mact);
    /* printf("mact %#x%x%x%x\n", mact[0], mact[1], mact[2], mact[3]); */
    g_assert_true(memcmp (mact, mact_x, 4) == 0);
}

static void test_eia2_TestSet3(){
    const guint8 countI[] = {0x36, 0xaf, 0x61, 0x44};
    const guint8 bearer = 0x18;
    const guint8 direction = 0x1;
    const guint8 ik[] = {0x7e, 0x5e, 0x94, 0x43, 0x1e, 0x11, 0xd7, 0x38,
                         0x28, 0xd7, 0x39, 0xcc, 0x6c, 0xed, 0x45, 0x73};

    const gsize len = 254;
    const guint8 msg[] = {0xb3, 0xd3, 0xc9, 0x17, 0x0a, 0x4e, 0x16, 0x32,
                          0xf6, 0x0f, 0x86, 0x10, 0x13, 0xd2, 0x2d, 0x84,
                          0xb7, 0x26, 0xb6, 0xa2, 0x78, 0xd8, 0x02, 0xd1,
                          0xee, 0xaf, 0x13, 0x21, 0xba, 0x59, 0x29, 0xdc};
    const guint8 mact_x[] = {0x1f, 0x60, 0xb0, 0x1d};
    guint8 mact[4] = {0};
    eia2(ik, countI, bearer, direction, msg, len, mact);
    /* printf("mact %#x%x%x%x\n", mact[0], mact[1], mact[2], mact[3]); */
    g_assert_true(memcmp (mact, mact_x, 4) == 0);
}

static void test_eia2_TestSet2(){
    const guint8 countI[] = {0x39, 0x8a, 0x59, 0xb4};
    const guint8 bearer = 0x1a;
    const guint8 direction = 0x1;
    const guint8 ik[] = {0xd3, 0xc5, 0xd5, 0x92, 0x32, 0x7f, 0xb1, 0x1c,
                         0x40, 0x35, 0xc6, 0x68, 0x0a, 0xf8, 0xc6, 0xd1};

    const gsize len = 64;
    const guint8 msg[] = {0x48, 0x45, 0x83, 0xd5, 0xaf, 0xe0, 0x82, 0xae};
    const guint8 mact_x[] = {0xb9, 0x37, 0x87, 0xe6};
    guint8 mact[4] = {0};
    eia2(ik, countI, bearer, direction, msg, len, mact);
    /* printf("mact %#x%x%x%x\n", mact[0], mact[1], mact[2], mact[3]); */
    g_assert_true(memcmp (mact, mact_x, 4) == 0);
}

static void test_eia2_TestSet1(){
    const guint8 countI[] = {0x38, 0xa6, 0xf0, 0x56};
    const guint8 bearer = 0x18;
    const guint8 direction = 0x0;
    const guint8 ik[] = {0x2b, 0xd6, 0x45, 0x9f, 0x82, 0xc5, 0xb3, 0x00,
                         0x95, 0x2c, 0x49, 0x10, 0x48, 0x81, 0xff, 0x48};

    const gsize len = 58;
    const guint8 msg[] = {0x33, 0x32, 0x34, 0x62, 0x63, 0x39, 0x38, 0x40};
    const guint8 mact_x[] = {0x11, 0x8c, 0x6e, 0xb8};
    guint8 mact[4] = {0};
    eia2(ik, countI, bearer, direction, msg, len, mact);
    /* printf("mact %#x%x%x%x\n", mact[0], mact[1], mact[2], mact[3]); */
    g_assert_true(memcmp (mact, mact_x, 4) == 0);
}

typedef struct {
    /* gpointer n; */
    NASHandler *n;
}NAS_Fixture;

/* Internal Functions*/
int nas_setCOUNTshort(const NAS h, const NAS_Direction direction, const uint8_t recv);

static void NAS_fixture_set_up(NAS_Fixture *nas, gconstpointer count){
    nas->n = nas_newHandler();
    nas->n->nas_count[0] = GPOINTER_TO_UINT(count);
}

static void NAS_fixture_tear_down(NAS_Fixture *nas, gconstpointer count){
    nas_freeHandler(nas->n);
}

static void test_nas_shortCount1(NAS_Fixture *nas, gconstpointer count){
    const guint8 c = GPOINTER_TO_UINT(count)&0x1F, next = (c+1)&0x1F;
    g_assert_cmpint(nas_getLastCount(nas->n, 0), ==, GPOINTER_TO_UINT(count)-1);
    nas_setCOUNTshort(nas->n, 0, c);
    g_assert_cmpint(nas_getLastCount(nas->n, 0), ==, GPOINTER_TO_UINT(count));
    nas_setCOUNTshort(nas->n, 0, next);
    g_assert_cmpint(nas_getLastCount(nas->n, 0), ==, GPOINTER_TO_UINT(count)+1);
}

static void test_dummy(int *n, gconstpointer data){
    g_assert(1==1);
}

int main (int argc, char **argv){
    g_test_init (&argc, &argv, NULL);
    g_test_add_func("/crypto/kdf", test_kdf_test1);
    g_test_add_func("/crypto/cmac", test_cmac);
    g_test_add_func("/crypto/eia2-ts1", test_eia2_TestSet1);
    g_test_add_func("/crypto/eia2-ts2", test_eia2_TestSet2);
    g_test_add_func("/crypto/eia2-ts3", test_eia2_TestSet3);
    g_test_add_func("/crypto/eia2-ts4", test_eia2_TestSet4);
    g_test_add_func("/crypto/eia2-ts5", test_eia2_TestSet5);
    g_test_add_func("/crypto/eia2-ts6", test_eia2_TestSet6);
    g_test_add("/nas/shortCount-in_byte_overflow1", NAS_Fixture, GUINT_TO_POINTER(0x3F),
               NAS_fixture_set_up, test_nas_shortCount1, NAS_fixture_tear_down);
    g_test_add("/nas/shortCount-in_byte_overflow2", NAS_Fixture, GUINT_TO_POINTER(0x13F),
               NAS_fixture_set_up, test_nas_shortCount1,
               NAS_fixture_tear_down);
    g_test_add("/nas/shortCount-byte_overflow1", NAS_Fixture, GUINT_TO_POINTER(0xFF),
               NAS_fixture_set_up, test_nas_shortCount1,
               NAS_fixture_tear_down);
    g_test_add("/nas/shortCount-byte_overflow2", NAS_Fixture, GUINT_TO_POINTER(0x1FF),
               NAS_fixture_set_up, test_nas_shortCount1,
               NAS_fixture_tear_down);

    return g_test_run();
}
