/*
 * Copyright (c) 2020 Belledonne Communications SARL.
 *
 * This file is part of bctoolbox.
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "bctoolbox/vfs.h"
#include "bctoolbox/logging.h"
#include "bctoolbox/vfs_standard.h"
#include "bctoolbox_tester.h"

static char *patterns[] = {
    "this is a small pattern",
    "O brillant éclat de la lampe d'argile, commodément suspendue dans cet endroit accessible aux regards, nous ferons "
    "connaître ta naissance et tes aventures; façonnée par la course de la roue du potier, tu portes dans tes narines "
    "les splendeurs éclatantes du soleil: produis donc au dehors le signal de ta flamme, comme il est convenu. A toi "
    "seule notre confiance; et nous avons raison, puisque, dans nos chambres, tu honores de ta présence nos essais de "
    "postures aphrodisiaques: témoin du mouvement de nos corps, personne n'écarte ton oeil de nos demeures. Seule tu "
    "éclaires les cavités secrètes de nos aines, brûlant la fleur de leur duvet. Ouvrons-nous furtivement des celliers "
    "pleins de fruits ou de liqueur bachique, tu es notre confidente, et ta complicité ne bavarde pas avec les "
    "voisins. Aussi connaîtras-tu les desseins actuels, que j'ai formés, à la fête des Skira, avec mes amies. "
    "Seulement, nulle ne se présente de celles qui devaient venir. Cependant voici l'aube: l'assemblée va se tenir "
    "dans un instant, et il nous faut prendre place, en dépit de Phyromakhos, qui, s'il vous en souvient, disait de "
    "nous: «Les femmes doivent avoir des sièges séparés et à l'écart.» Que peut-il être arrivé? N'ont-elles pas dérobé "
    "les barbes postiches, qu'on avait promis d'avoir, ou leur a-t-il été difficile de voler en secret les manteaux de "
    "leurs maris? Ah! je vois une lumière qui s'avance: retirons-nous un peu, dans la crainte que ce ne soit quelque "
    "homme qui approche par ici."};

// convenience define for test buffer size
#define F_SIZE 2 * BCTBX_VFS_PRINTF_PAGE_SIZE
#define G_SIZE BCTBX_VFS_GETLINE_PAGE_SIZE - 1

void file_fprint_simple_test() {
	char in_buf[F_SIZE];
	char out_buf[F_SIZE];
	memset(in_buf, 0, F_SIZE);
	memset(out_buf, 0, F_SIZE);

	/* create a file */
	char *path = bc_tester_file("vfs_fprintf_simple.txt");
	remove(path);                                                                    // make sure it does not exist
	bctbx_vfs_file_t *fp = bctbx_file_open2(&bcStandardVfs, path, O_RDWR | O_CREAT); // open using standard vfs
	BC_ASSERT_PTR_NOT_NULL(fp);

	/* small printf at offset 0 */
	BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s", patterns[0]) - strlen(patterns[0]) == 0);

	/* read it */
	ssize_t readSize = bctbx_file_read(fp, out_buf, 100, 0);
	BC_ASSERT_EQUAL(readSize - strlen(patterns[0]), 0, int, "%d");
	BC_ASSERT_TRUE(memcmp(patterns[0], out_buf, readSize) == 0);
	memset(out_buf, 0, F_SIZE);

	/* close the file, re-open and read it again */
	BC_ASSERT_NOT_EQUAL(bctbx_file_close(fp), BCTBX_VFS_ERROR, int, "%d");
	fp = bctbx_file_open2(&bcStandardVfs, path, O_RDWR | O_CREAT); // open using standard vfs
	BC_ASSERT_PTR_NOT_NULL(fp);
	readSize = bctbx_file_read(fp, out_buf, F_SIZE, 0);
	BC_ASSERT_EQUAL(readSize - strlen(patterns[0]), 0, int, "%d");
	BC_ASSERT_TRUE(memcmp(patterns[0], out_buf, readSize) == 0);
	memset(out_buf, 0, F_SIZE);

	/* write a large quantity of small inputs - less than a cache page */
	size_t inSize = 0;
	while (inSize < BCTBX_VFS_PRINTF_PAGE_SIZE / 2) {
		sprintf(in_buf + inSize, "%s", patterns[0]); // build a buffer image of what we are writing in the file
		BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s", patterns[0]) - strlen(patterns[0]) == 0);
		inSize += strlen(patterns[0]);
	}
	/* read it (it flushes the write cache) */
	readSize = bctbx_file_read(fp, out_buf, F_SIZE, 0);
	BC_ASSERT_TRUE((readSize - inSize) == 0);
	BC_ASSERT_TRUE(memcmp(in_buf, out_buf, readSize) == 0);
	memset(out_buf, 0, F_SIZE);

	/* reset the file pointer, and write 3 times the whole half page buffer - more than a cache page */
	bctbx_file_seek(fp, 0, SEEK_SET);
	BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s", in_buf) - inSize == 0);
	BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s", in_buf) - inSize == 0);
	BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s", in_buf) - inSize == 0);
	readSize = bctbx_file_read(fp, out_buf, F_SIZE, 0);
	BC_ASSERT_TRUE((readSize - 3 * inSize) == 0);
	BC_ASSERT_TRUE(memcmp(in_buf, out_buf, inSize) == 0);
	BC_ASSERT_TRUE(memcmp(in_buf, out_buf + inSize, inSize) == 0);
	BC_ASSERT_TRUE(memcmp(in_buf, out_buf + 2 * inSize, inSize) == 0);
	memcpy(in_buf, out_buf, BCTBX_VFS_PRINTF_PAGE_SIZE); // in_buf has now 1 cache page size
	in_buf[BCTBX_VFS_PRINTF_PAGE_SIZE] = '\0';
	memset(out_buf, 0, F_SIZE);

	/* write exactly a page size in one write - the read flushed the cache, so we start we an empty one */
	BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s", in_buf) - BCTBX_VFS_PRINTF_PAGE_SIZE == 0);
	readSize = bctbx_file_read(fp, out_buf, F_SIZE,
	                           readSize); // offset a readSize, so we start reading at the end of what we read before
	BC_ASSERT_TRUE((readSize - BCTBX_VFS_PRINTF_PAGE_SIZE) == 0);
	BC_ASSERT_TRUE(memcmp(in_buf, out_buf, BCTBX_VFS_PRINTF_PAGE_SIZE) == 0);
	BC_ASSERT_TRUE(bctbx_file_size(fp) - 3 * inSize - BCTBX_VFS_PRINTF_PAGE_SIZE == 0);
	memset(out_buf, 0, F_SIZE);

	/* delete file and reopen it */
	BC_ASSERT_NOT_EQUAL(bctbx_file_close(fp), BCTBX_VFS_ERROR, int, "%d");
	remove(path);
	fp = bctbx_file_open2(&bcStandardVfs, path, O_RDWR | O_CREAT); // open using standard vfs
	BC_ASSERT_PTR_NOT_NULL(fp);

	/* write a whole page - 1, then a small buffer, read it all */
	in_buf[BCTBX_VFS_PRINTF_PAGE_SIZE - 1] = '\0';
	BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s", in_buf) - BCTBX_VFS_PRINTF_PAGE_SIZE + 1 == 0);
	BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s", patterns[0]) - strlen(patterns[0]) == 0);
	readSize = bctbx_file_read(fp, out_buf, F_SIZE, 0);
	BC_ASSERT_TRUE((readSize - BCTBX_VFS_PRINTF_PAGE_SIZE + 1 - strlen(patterns[0])) == 0);
	BC_ASSERT_TRUE(memcmp(in_buf, out_buf, BCTBX_VFS_PRINTF_PAGE_SIZE - 1) == 0);
	BC_ASSERT_TRUE(memcmp(patterns[0], out_buf + BCTBX_VFS_PRINTF_PAGE_SIZE - 1, strlen(patterns[0])) == 0);

	/* delete file and reopen it */
	BC_ASSERT_NOT_EQUAL(bctbx_file_close(fp), BCTBX_VFS_ERROR, int, "%d");
	remove(path);
	fp = bctbx_file_open2(&bcStandardVfs, path, O_RDWR | O_CREAT); // open using standard vfs
	BC_ASSERT_PTR_NOT_NULL(fp);

	/* write a small buffer and then a whole page - 1  */
	BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s", patterns[0]) - strlen(patterns[0]) == 0);
	BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s", in_buf) - BCTBX_VFS_PRINTF_PAGE_SIZE + 1 == 0);
	readSize = bctbx_file_read(fp, out_buf, F_SIZE, 0);
	BC_ASSERT_TRUE((readSize - BCTBX_VFS_PRINTF_PAGE_SIZE + 1 - strlen(patterns[0])) == 0);
	BC_ASSERT_TRUE(memcmp(patterns[0], out_buf, strlen(patterns[0])) == 0);
	BC_ASSERT_TRUE(memcmp(in_buf, out_buf + strlen(patterns[0]), BCTBX_VFS_PRINTF_PAGE_SIZE - 1) == 0);

	/* cleaning */
	BC_ASSERT_NOT_EQUAL(bctbx_file_close(fp), BCTBX_VFS_ERROR, int, "%d");
	remove(path);
	bctbx_free(path);
}

void file_fprint_and_write_test() {
	char out_buf[F_SIZE];
	memset(out_buf, 0, F_SIZE);

	/* create a file */
	char *path = bc_tester_file("vfs_fprintf_mixed.txt");
	remove(path);                                                                    // make sure it does not exist
	bctbx_vfs_file_t *fp = bctbx_file_open2(&bcStandardVfs, path, O_RDWR | O_CREAT); // open using standard vfs
	BC_ASSERT_PTR_NOT_NULL(fp);

	/* printf at offset 0 - fits in cache, so it won't make it to the file */
	BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s", patterns[1]) - strlen(patterns[1]) == 0);
	/* write at offset 0 too, expected to overwrite what was printed before */
	BC_ASSERT_TRUE(bctbx_file_write(fp, patterns[0], strlen(patterns[0]), 0) - strlen(patterns[0]) == 0);
	/* read all and check */
	ssize_t readSize = bctbx_file_read(fp, out_buf, F_SIZE, 0);
	BC_ASSERT_TRUE(readSize - strlen(patterns[1]) == 0);
	out_buf[strlen(patterns[1])] = '\0';
	BC_ASSERT_TRUE(memcmp(out_buf, patterns[0], strlen(patterns[0])) == 0);
	BC_ASSERT_TRUE(memcmp(out_buf + strlen(patterns[0]), patterns[1] + strlen(patterns[0]),
	                      strlen(patterns[1]) - strlen(patterns[0])) == 0);

	/* cleaning */
	BC_ASSERT_NOT_EQUAL(bctbx_file_close(fp), BCTBX_VFS_ERROR, int, "%d");
	remove(path);
	bctbx_free(path);
}

void file_get_nxtline_test() {
	// char in_buf[F_SIZE];
	char out_buf[2 * G_SIZE];
	// memset(in_buf, 0, F_SIZE);
	memset(out_buf, 0, 2 * G_SIZE);

	/* create a file */
	char *path = bc_tester_file("vfs_get_nxtline.txt");
	remove(path);                                                                    // make sure it does not exist
	bctbx_vfs_file_t *fp = bctbx_file_open2(&bcStandardVfs, path, O_RDWR | O_CREAT); // open using standard vfs
	BC_ASSERT_PTR_NOT_NULL(fp);

	/* write a test file using fprintf, alternate line ending with \n, \r or \r\n */
	BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s\n", patterns[0]) - strlen(patterns[0]) - 1 == 0);
	BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s\r", patterns[1]) - strlen(patterns[1]) - 1 == 0);
	BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s\r\n", patterns[0]) - strlen(patterns[0]) - 2 == 0);
	BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s\n", patterns[1]) - strlen(patterns[1]) - 1 == 0);
	BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "\n") - 1 == 0); // empty line
	/* parse all the lines */
	bctbx_file_seek(fp, 0, SEEK_SET); // reset print/get file pointer
	BC_ASSERT_TRUE(bctbx_file_get_nxtline(fp, out_buf, G_SIZE) - strlen(patterns[0]) - 1 == 0);
	BC_ASSERT_NSTRING_EQUAL(out_buf, patterns[0], strlen(patterns[0]));
	BC_ASSERT_TRUE(bctbx_file_get_nxtline(fp, out_buf, G_SIZE) - strlen(patterns[1]) - 1 == 0);
	BC_ASSERT_NSTRING_EQUAL(out_buf, patterns[1], strlen(patterns[1]));
	BC_ASSERT_TRUE(bctbx_file_get_nxtline(fp, out_buf, G_SIZE) - strlen(patterns[0]) - 1 == 0);
	BC_ASSERT_NSTRING_EQUAL(out_buf, patterns[0], strlen(patterns[0]));
	BC_ASSERT_TRUE(bctbx_file_get_nxtline(fp, out_buf, G_SIZE) - strlen(patterns[1]) - 1 == 0);
	BC_ASSERT_NSTRING_EQUAL(out_buf, patterns[1], strlen(patterns[1]));
	BC_ASSERT_TRUE(bctbx_file_get_nxtline(fp, out_buf, G_SIZE) - 1 == 0);
	/* cleaning */
	bctbx_file_close(fp);
	remove(path);
	fp = bctbx_file_open2(&bcStandardVfs, path, O_RDWR | O_CREAT); // open using standard vfs
	BC_ASSERT_PTR_NOT_NULL(fp);

	/* write a file larger than a read cache page, with lines ending with \n */
	size_t written = 0;
	int i, count = 0;
	while (written < BCTBX_VFS_GETLINE_PAGE_SIZE + 1000) {
		BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s\n", patterns[0]) - strlen(patterns[0]) - 1 == 0);
		BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s\n", patterns[1]) - strlen(patterns[1]) - 1 == 0);
		count++;
		written += strlen(patterns[0]) + strlen(patterns[1]) + 2;
	}
	/* parse it */
	bctbx_file_seek(fp, 0, SEEK_SET); // reset print/get file pointer
	for (i = 0; i < count; i++) {
		BC_ASSERT_TRUE(bctbx_file_get_nxtline(fp, out_buf, G_SIZE) - strlen(patterns[0]) - 1 == 0);
		BC_ASSERT_NSTRING_EQUAL(out_buf, patterns[0], strlen(patterns[0]));
		BC_ASSERT_TRUE(bctbx_file_get_nxtline(fp, out_buf, G_SIZE) - strlen(patterns[1]) - 1 == 0);
		BC_ASSERT_NSTRING_EQUAL(out_buf, patterns[1], strlen(patterns[1]));
	}

	/* cleaning */
	bctbx_file_close(fp);
	remove(path);
	fp = bctbx_file_open2(&bcStandardVfs, path, O_RDWR | O_CREAT); // open using standard vfs
	BC_ASSERT_PTR_NOT_NULL(fp);

	/* write a file with one line larger than read cache */
	written = 0;
	count = 0;
	while (written < BCTBX_VFS_GETLINE_PAGE_SIZE + 1000) {
		BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s", patterns[1]) - strlen(patterns[1]) == 0);
		count++;
		written += strlen(patterns[1]);
	}
	BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "\n") - 1 == 0); // empty line
	/* parse it */
	bctbx_file_seek(fp, 0, SEEK_SET); // reset print/get file pointer
	BC_ASSERT_TRUE(
	    bctbx_file_get_nxtline(fp, out_buf, BCTBX_VFS_GETLINE_PAGE_SIZE) - (BCTBX_VFS_GETLINE_PAGE_SIZE - 1) ==
	    0); // read the size of a page cache, there is no \n in it, but it cannot send more data than given buffer -1
	bctbx_file_seek(fp, 0, SEEK_SET); // reset print/get file pointer
	BC_ASSERT_TRUE(bctbx_file_get_nxtline(fp, out_buf, 2 * G_SIZE) - (written + 1) ==
	               0); // read more, we shall get a warning in the logs but the whole line in return
	for (i = 0; i < count; i++) {
		BC_ASSERT_TRUE(memcmp(out_buf + i * strlen(patterns[1]), patterns[1], strlen(patterns[1])) == 0);
	}

	/* cleaning */
	bctbx_file_close(fp);
	remove(path);
	fp = bctbx_file_open2(&bcStandardVfs, path, O_RDWR | O_CREAT); // open using standard vfs
	BC_ASSERT_PTR_NOT_NULL(fp);

	/* write a file with one line exactly the size of read cache */
	written = 0;
	count = 0;
	while (written < BCTBX_VFS_GETLINE_PAGE_SIZE - strlen(patterns[1])) {
		BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s", patterns[1]) - strlen(patterns[1]) == 0);
		count++;
		written += strlen(patterns[1]);
	}
	out_buf[BCTBX_VFS_GETLINE_PAGE_SIZE - written - 1] = '\0';
	BC_ASSERT_TRUE(bctbx_file_fprintf(fp, 0, "%s\n", out_buf) - strlen(out_buf) - 1 == 0);
	BC_ASSERT_TRUE(bctbx_file_size(fp) - BCTBX_VFS_GETLINE_PAGE_SIZE == 0);
	/* parse it */
	bctbx_file_seek(fp, 0, SEEK_SET); // reset print/get file pointer
	BC_ASSERT_TRUE(bctbx_file_get_nxtline(fp, out_buf, 10) - 9 ==
	               0);                // small read to load the cache, max buf is 10, so 9 char will be read
	bctbx_file_seek(fp, 0, SEEK_SET); // reset print/get file pointer - this has no effect on the read cache
	BC_ASSERT_TRUE(bctbx_file_get_nxtline(fp, out_buf, BCTBX_VFS_GETLINE_PAGE_SIZE) -
	                   (BCTBX_VFS_GETLINE_PAGE_SIZE - 1) ==
	               0); // read more, we shall get a warning in the logs but the whole line in return

	/* cleaning */
	bctbx_file_close(fp);
	remove(path);
	bctbx_free(path);
}

static test_t vfs_tests[] = {TEST_NO_TAG("File fprint - simple", file_fprint_simple_test),
                             TEST_NO_TAG("File fprint and file_write mixed", file_fprint_and_write_test),
                             TEST_NO_TAG("File get next line", file_get_nxtline_test)};


test_suite_t vfs_test_suite = {"vfs", NULL, NULL, NULL, NULL, sizeof(vfs_tests) / sizeof(vfs_tests[0]), vfs_tests, 0};