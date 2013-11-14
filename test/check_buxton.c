/*
 * This file is part of buxton.
 *
 * Copyright (C) 2013 Intel Corporation
 *
 * buxton is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#include <check.h>
#include <stdlib.h>
#include <unistd.h>

#include "backend.h"
#include "bt-daemon.h"
#include "check_utils.h"
#include "protocol.h"
#include "serialize.h"
#include "util.h"

START_TEST(buxton_direct_open_check)
{
	BuxtonClient c;
	fail_if(buxton_direct_open(&c) == false,
		"Direct open failed without daemon.");
}
END_TEST

START_TEST(buxton_direct_set_value_check)
{
	BuxtonClient c;
	fail_if(buxton_direct_open(&c) == false,
		"Direct open failed without daemon.");
	BuxtonData data;
	BuxtonString layer = buxton_string_pack("test-gdbm");
	BuxtonString key = buxton_string_pack("bxt_test");
	data.type = STRING;
	data.label = buxton_string_pack("label");
	data.store.d_string = buxton_string_pack("bxt_test_value");
	fail_if(buxton_client_set_value(&c, &layer, &key, &data) == false,
		"Setting value in buxton directly failed.");
}
END_TEST

START_TEST(buxton_direct_get_value_for_layer_check)
{
	BuxtonClient c;
	BuxtonData result;
	BuxtonString layer = buxton_string_pack("test-gdbm");
	BuxtonString key = buxton_string_pack("bxt_test");
	fail_if(buxton_direct_open(&c) == false,
		"Direct open failed without daemon.");
	fail_if(buxton_client_get_value_for_layer(&c, &layer, &key, &result) == false,
		"Retrieving value from buxton gdbm backend failed.");
	fail_if(result.type != STRING,
		"Buxton gdbm backend returned incorrect result type.");
	fail_if(strcmp(result.label.value, "label") != 0,
		"Buxton gdbm returned a different label to that set.");
	fail_if(strcmp(result.store.d_string.value, "bxt_test_value") != 0,
		"Buxton gdbm returned a different value to that set.");
	if (result.label.value)
		free(result.label.value);
	if (result.store.d_string.value)
		free(result.store.d_string.value);
}
END_TEST

START_TEST(buxton_direct_get_value_check)
{
	BuxtonClient c;
	BuxtonData data, result;
	BuxtonString layer = buxton_string_pack("test-gdbm-user");
	BuxtonString key = buxton_string_pack("bxt_test");
	fail_if(buxton_direct_open(&c) == false,
		"Direct open failed without daemon.");

	data.type = STRING;
	data.label = buxton_string_pack("label2");
	data.store.d_string = buxton_string_pack("bxt_test_value2");
	fail_if(data.store.d_string.value == NULL,
		"Failed to allocate test string.");
	fail_if(buxton_client_set_value(&c, &layer, &key, &data) == false,
		"Failed to set second value.");
	fail_if(buxton_client_get_value(&c, &key, &result) == false,
		"Retrieving value from buxton gdbm backend failed.");
	fail_if(result.type != STRING,
		"Buxton gdbm backend returned incorrect result type.");
	fail_if(strcmp(result.label.value, "label2") != 0,
		"Buxton gdbm returned a different label to that set.");
	fail_if(strcmp(result.store.d_string.value, "bxt_test_value2") != 0,
		"Buxton gdbm returned a different value to that set.");
	if (result.label.value)
		free(result.label.value);
	if (result.store.d_string.value)
		free(result.store.d_string.value);
}
END_TEST

START_TEST(buxton_memory_backend_check)
{
	BuxtonClient c;
	BuxtonString layer = buxton_string_pack("temp");
	BuxtonString key = buxton_string_pack("bxt_mem_test");
	fail_if(buxton_direct_open(&c) == false,
		"Direct open failed without daemon.");
	BuxtonData data, result;
	data.type = STRING;
	data.label = buxton_string_pack("label");
	data.store.d_string = buxton_string_pack("bxt_test_value");
	fail_if(buxton_client_set_value(&c, &layer, &key, &data) == false,
		"Setting value in buxton memory backend directly failed.");
	fail_if(buxton_client_get_value_for_layer(&c, &layer, &key, &result) == false,
		"Retrieving value from buxton memory backend directly failed.");
	fail_if(!streq(result.label.value, "label"),
		"Buxton memory returned a different label to that set.");
	fail_if(!streq(result.store.d_string.value, "bxt_test_value"),
		"Buxton memory returned a different value to that set.");
}
END_TEST

START_TEST(buxton_wire_get_response_check)
{
	BuxtonClient client;
	BuxtonData *list = NULL;
	BuxtonControlMessage msg;
	pid_t pid;
	int server;

	setup_socket_pair(&(client.fd), &server);

	pid = fork();
	if (pid == 0) {
		/* child (server) */
		uint8_t *dest = NULL;
		size_t size;
		BuxtonData data;
		close(client.fd);
		data.type = INT;
		data.store.d_int = 0;
		data.label = buxton_string_pack("dummy");
		size = buxton_serialize_message(&dest, BUXTON_CONTROL_STATUS, 1, &data);
		fail_if(size == 0, "Failed to serialize message");
		write(server, dest, size);
		close(server);
		_exit(EXIT_SUCCESS);
	} else if (pid == -1) {
		/* error */
		fail("Failed to fork for response check");
	} else {
		/* parent (client) */
		close(server);
		fail_if(buxton_wire_get_response(&client, &msg, &list) != 1,
			"Failed to properly handle response");
		fail_if(msg != BUXTON_CONTROL_STATUS,
			"Failed to get correct control message type");
		fail_if(list[0].type != INT,
			"Failed to get correct data type from message");
		fail_if(!(list[0].label.value),
			"Failed to get label from message");
		fail_if(!streq(list[0].label.value, "dummy"),
			"Failed to get correct label from message");
		fail_if(list[0].store.d_int != 0,
			"Failed to get correct data value from message");
		free(list);
		close(client.fd);
	}
}
END_TEST

START_TEST(buxton_wire_set_value_check)
{
	BuxtonClient client;
	pid_t pid;
	int server;

	setup_socket_pair(&(client.fd), &server);

	pid = fork();
	if (pid == 0) {
		/* child (server) */
		uint8_t *dest = NULL;
		size_t size;
		BuxtonData data;
		uint8_t buf[4096];
		ssize_t r;

		close(client.fd);
		data.type = INT;
		data.store.d_int = 0;
		data.label = buxton_string_pack("dummy");
		size = buxton_serialize_message(&dest, BUXTON_CONTROL_STATUS, 1, &data);
		fail_if(size == 0, "Failed to serialize message");
		r = read(server, buf, 4096);
		fail_if(r < 0, "Read failed from buxton_wire_set_value");
		write(server, dest, size);
		close(server);
		free(dest);
		_exit(EXIT_SUCCESS);
	} else if (pid == -1) {
		/* error */
		fail("Failed to fork for set check");
	} else {
		/* parent (client) */
		BuxtonString layer_name, key;
		BuxtonData value;

		close(server);
		layer_name = buxton_string_pack("layer");
		key = buxton_string_pack("key");
		value.type = STRING;
		value.label = buxton_string_pack("label");
		value.store.d_string = buxton_string_pack("value");
		fail_if(buxton_wire_set_value(&client, &layer_name, &key, &value) != true,
			"Failed to properly set value");
		close(client.fd);
	}
}
END_TEST

START_TEST(buxton_wire_get_value_check)
{
	BuxtonClient client;
	pid_t pid;
	int server;

	setup_socket_pair(&(client.fd), &server);

	pid = fork();
	if (pid == 0) {
		/* child (server) */
		uint8_t *dest = NULL;
		size_t size;
		BuxtonData data1, data2;
		uint8_t buf[4096];
		ssize_t r;

		close(client.fd);
		data1.type = INT;
		data1.store.d_int = 0;
		data1.label = buxton_string_pack("dummy");
		data2.type = INT;
		data2.store.d_int = 1;
		data2.label = buxton_string_pack("label");
		size = buxton_serialize_message(&dest, BUXTON_CONTROL_STATUS, 2, &data1,
						&data2);
		fail_if(size == 0, "Failed to serialize message");
		r = read(server, buf, 4096);
		fail_if(r < 0, "Read failed from buxton_wire_get_value");
		write(server, dest, size);
		close(server);
		free(dest);
		_exit(EXIT_SUCCESS);
	} else if (pid == -1) {
		/* error */
		fail("Failed to fork for get check");
	} else {
		/* parent (client) */
		BuxtonString layer_name, key;
		BuxtonData value;

		close(server);
		layer_name = buxton_string_pack("layer");
		key = buxton_string_pack("key");
		fail_if(buxton_wire_get_value(&client, &layer_name, &key, &value) != true,
			"Failed to properly get value");
		fail_if(value.type != INT, "Failed to get value's correct type");
		fail_if(!(value.label.value), "Failed to get value's label");
		fail_if(!streq(value.label.value, "label"),
			"Failed to get value's correct label");
		fail_if(value.store.d_int != 1, "Failed to get value's correct value");
		close(client.fd);
		free(value.label.value);
	}
}
END_TEST

static Suite *
buxton_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("buxton");

	tc = tcase_create("buxton_client_lib_functions");
	tcase_add_test(tc, buxton_direct_open_check);
	tcase_add_test(tc, buxton_direct_set_value_check);
	tcase_add_test(tc, buxton_direct_get_value_for_layer_check);
	tcase_add_test(tc, buxton_direct_get_value_check);
	tcase_add_test(tc, buxton_memory_backend_check);
	suite_add_tcase(s, tc);

	tc = tcase_create("buxton_protocol_functions");
	tcase_add_test(tc, buxton_wire_get_response_check);
	tcase_add_test(tc, buxton_wire_set_value_check);
	tcase_add_test(tc, buxton_wire_get_value_check);
	suite_add_tcase(s, tc);

	return s;
}

int main(void)
{
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = buxton_suite();
	sr = srunner_create(s);
	srunner_run_all(sr, CK_VERBOSE);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}


/*
 * Editor modelines  -  http://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 *
 * vi: set shiftwidth=8 tabstop=8 noexpandtab:
 * :indentSize=8:tabSize=8:noTabs=false:
 */
