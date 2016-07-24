#!/usr/bin/python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import argparse
import logging
import os
import re
import subprocess
import sys
import tempfile
import requests


class OpenmanoCommandFailed(Exception):
    pass


class OpenmanoUnexpectedOutput(Exception):
    pass


class VNFExistsError(Exception):
    pass


class InstanceStatusError(Exception):
    pass


class OpenmanoHttpAPI(object):
    def __init__(self, log, host, port, tenant):
        self._log = log
        self._host = host
        self._port = port
        self._tenant = tenant

        self._session = requests.Session()

    def get_instance(self, instance_uuid):
        url = "http://{host}:{port}/openmano/{tenant}/instances/{instance}".format(
                host=self._host,
                port=self._port,
                tenant=self._tenant,
                instance=instance_uuid,
                )

        resp = self._session.get(url)
        try:
            resp.raise_for_status()
        except requests.exceptions.HTTPError as e:
            raise InstanceStatusError(e)

        return resp.json()


class OpenmanoCliAPI(object):
    """ This class implements the necessary funtionality to interact with  """

    CMD_TIMEOUT = 15

    def __init__(self, log, host, port, tenant):
        self._log = log
        self._host = host
        self._port = port
        self._tenant = tenant

    @staticmethod
    def openmano_cmd_path():
        return os.path.join(
               os.environ["RIFT_INSTALL"],
               "usr/bin/openmano"
               )

    def _openmano_cmd(self, arg_list, expected_lines=None):
        cmd_args = list(arg_list)
        cmd_args.insert(0, self.openmano_cmd_path())

        env = {
                "OPENMANO_HOST": self._host,
                "OPENMANO_PORT": str(self._port),
                "OPENMANO_TENANT": self._tenant,
                }

        self._log.debug(
                "Running openmano command (%s) using env (%s)",
                subprocess.list2cmdline(cmd_args),
                env,
                )

        proc = subprocess.Popen(
                cmd_args,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                env=env
                )
        try:
            stdout, stderr = proc.communicate(timeout=self.CMD_TIMEOUT)
        except subprocess.TimeoutExpired:
            self._log.error("Openmano command timed out")
            proc.terminate()
            stdout, stderr = proc.communicate(timeout=self.CMD_TIMEOUT)

        if proc.returncode != 0:
            self._log.error(
                    "Openmano command failed (rc=%s) with stdout: %s",
                    proc.returncode, stdout
                    )
            raise OpenmanoCommandFailed(stdout)

        self._log.debug("Openmano command completed with stdout: %s", stdout)

        output_lines = stdout.splitlines()
        if expected_lines is not None:
            if len(output_lines) != expected_lines:
                msg = "Expected %s lines from openmano command. Got %s" % (expected_lines, len(output_lines))
                self._log.error(msg)
                raise OpenmanoUnexpectedOutput(msg)

        return output_lines


    def vnf_create(self, vnf_yaml_str):
        """ Create a Openmano VNF from a Openmano VNF YAML string """

        self._log.debug("Creating VNF: %s", vnf_yaml_str)

        with tempfile.NamedTemporaryFile() as vnf_file_hdl:
            vnf_file_hdl.write(vnf_yaml_str.encode())
            vnf_file_hdl.flush()

            try:
                output_lines = self._openmano_cmd(
                        ["vnf-create", vnf_file_hdl.name],
                        expected_lines=1
                        )
            except OpenmanoCommandFailed as e:
                if "already in use" in str(e):
                    raise VNFExistsError("VNF was already added")
                raise

        vnf_info_line = output_lines[0]
        vnf_id, vnf_name = vnf_info_line.split()

        self._log.info("VNF %s Created: %s", vnf_name, vnf_id)

        return vnf_id, vnf_name

    def vnf_delete(self, vnf_uuid):
        self._openmano_cmd(
                ["vnf-delete", vnf_uuid, "-f"],
                )

        self._log.info("VNF Deleted: %s", vnf_uuid)

    def vnf_list(self):
        try:
            output_lines = self._openmano_cmd(
                    ["vnf-list"],
                    )
        except OpenmanoCommandFailed as e:
            self._log.warning("Vnf listing returned an error: %s", str(e))
            return {}

        name_uuid_map = {}
        for line in output_lines:
            line = line.strip()
            uuid, name = line.split()
            name_uuid_map[name] = uuid

        return name_uuid_map

    def ns_create(self, ns_yaml_str, name=None):
        self._log.info("Creating NS: %s", ns_yaml_str)

        with tempfile.NamedTemporaryFile() as ns_file_hdl:
            ns_file_hdl.write(ns_yaml_str.encode())
            ns_file_hdl.flush()

            cmd_args = ["scenario-create", ns_file_hdl.name]
            if name is not None:
                cmd_args.extend(["--name", name])

            output_lines = self._openmano_cmd(
                    cmd_args,
                    expected_lines=1
                    )

        ns_info_line = output_lines[0]
        ns_id, ns_name = ns_info_line.split()

        self._log.info("NS %s Created: %s", ns_name, ns_id)

        return ns_id, ns_name

    def ns_list(self):
        self._log.debug("Getting NS list")

        try:
            output_lines = self._openmano_cmd(
                    ["scenario-list"],
                    )

        except OpenmanoCommandFailed as e:
            self._log.warning("NS listing returned an error: %s", str(e))
            return {}

        name_uuid_map = {}
        for line in output_lines:
            line = line.strip()
            uuid, name = line.split()
            name_uuid_map[name] = uuid

        return name_uuid_map

    def ns_delete(self, ns_uuid):
        self._log.info("Deleting NS: %s", ns_uuid)

        self._openmano_cmd(
                ["scenario-delete", ns_uuid, "-f"],
                )

        self._log.info("NS Deleted: %s", ns_uuid)

    def ns_instance_list(self):
        self._log.debug("Getting NS instance list")

        try:
            output_lines = self._openmano_cmd(
                    ["instance-scenario-list"],
                    )

        except OpenmanoCommandFailed as e:
            self._log.warning("Instance scenario listing returned an error: %s", str(e))
            return {}

        if "No scenario instances were found" in output_lines[0]:
            self._log.debug("No openmano instances were found")
            return {}

        name_uuid_map = {}
        for line in output_lines:
            line = line.strip()
            uuid, name = line.split()
            name_uuid_map[name] = uuid

        return name_uuid_map


    def ns_instantiate(self, scenario_name, instance_name, datacenter_name=None):
        self._log.info(
                "Instantiating NS %s using instance name %s",
                scenario_name,
                instance_name,
                )

        cmd_args = ["scenario-deploy", scenario_name, instance_name]
        if datacenter_name is not None:
            cmd_args.extend(["--datacenter", datacenter_name])

        output_lines = self._openmano_cmd(
                cmd_args,
                expected_lines=4
                )

        uuid, _ = output_lines[0].split()

        self._log.info("NS Instance Created: %s", uuid)

        return uuid

    def ns_terminate(self, ns_instance_name):
        self._log.info("Terminating NS: %s", ns_instance_name)

        self._openmano_cmd(
                ["instance-scenario-delete", ns_instance_name, "-f"],
                )

        self._log.info("NS Instance Deleted: %s", ns_instance_name)

    def datacenter_list(self):
        lines = self._openmano_cmd(["datacenter-list",])

        # The results returned from openmano are formatted with whitespace and
        # datacenter names may contain whitespace as well, so we use a regular
        # expression to parse each line of the results return from openmano to
        # extract the uuid and name of a datacenter.
        hex = '[0-9a-fA-F]'
        uuid_pattern = '(xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)'.replace('x', hex)
        name_pattern = '(.+?)'
        datacenter_regex = re.compile(r'{uuid}\s+\b{name}\s*$'.format(
            uuid=uuid_pattern,
            name=name_pattern,
            ))

        # Parse the results for the datacenter uuids and names
        datacenters = list()
        for line in lines:
            result = datacenter_regex.match(line)
            if result is not None:
                uuid, name = result.groups()
                datacenters.append((uuid, name))

        return datacenters


def valid_uuid(uuid_str):
    uuid_re = re.compile(
            "^xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx$".replace('x', '[0-9a-fA-F]')
            )

    if not uuid_re.match(uuid_str):
        raise argparse.ArgumentTypeError("Got a valid uuid: %s" % uuid_str)

    return uuid_str


def parse_args(argv=sys.argv[1:]):
    """ Parse the command line arguments

    Arguments:
        argv - The list of arguments to parse

    Returns:
        Argparse Namespace instance
    """
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-d', '--host',
        default='localhost',
        help="Openmano host/ip",
        )

    parser.add_argument(
        '-p', '--port',
        default='9090',
        help="Openmano port",
        )

    parser.add_argument(
        '-t', '--tenant',
        required=True,
        type=valid_uuid,
        help="Openmano tenant uuid to use",
        )

    subparsers = parser.add_subparsers(dest='command', help='openmano commands')

    vnf_create_parser = subparsers.add_parser(
            'vnf-create',
            help="Adds a openmano vnf into the catalog"
            )
    vnf_create_parser.add_argument(
            "file",
            help="location of the JSON file describing the VNF",
            type=argparse.FileType('rb'),
            )

    vnf_delete_parser = subparsers.add_parser(
            'vnf-delete',
            help="Deletes a openmano vnf into the catalog"
            )
    vnf_delete_parser.add_argument(
            "uuid",
            help="The vnf to delete",
            type=valid_uuid,
            )


    ns_create_parser = subparsers.add_parser(
            'scenario-create',
            help="Adds a openmano ns scenario into the catalog"
            )
    ns_create_parser.add_argument(
            "file",
            help="location of the JSON file describing the NS",
            type=argparse.FileType('rb'),
            )

    ns_delete_parser = subparsers.add_parser(
            'scenario-delete',
            help="Deletes a openmano ns into the catalog"
            )
    ns_delete_parser.add_argument(
            "uuid",
            help="The ns to delete",
            type=valid_uuid,
            )


    ns_instance_create_parser = subparsers.add_parser(
            'scenario-deploy',
            help="Deploys a openmano ns scenario into the catalog"
            )
    ns_instance_create_parser.add_argument(
            "scenario_name",
            help="The ns scenario name to deploy",
            )
    ns_instance_create_parser.add_argument(
            "instance_name",
            help="The ns instance name to deploy",
            )


    ns_instance_delete_parser = subparsers.add_parser(
            'instance-scenario-delete',
            help="Deploys a openmano ns scenario into the catalog"
            )
    ns_instance_delete_parser.add_argument(
            "instance_name",
            help="The ns instance name to delete",
            )


    _ = subparsers.add_parser(
            'datacenter-list',
            )

    args = parser.parse_args(argv)

    return args


def main():
    logging.basicConfig(level=logging.DEBUG)
    logger = logging.getLogger("openmano_client.py")

    if "RIFT_INSTALL" not in os.environ:
        logger.error("Must be in rift-shell to run.")
        sys.exit(1)

    args = parse_args()
    openmano_cli = OpenmanoCliAPI(logger, args.host, args.port, args.tenant)

    if args.command == "vnf-create":
        openmano_cli.vnf_create(args.file.read())

    elif args.command == "vnf-delete":
        openmano_cli.vnf_delete(args.uuid)

    elif args.command == "scenario-create":
        openmano_cli.ns_create(args.file.read())

    elif args.command == "scenario-delete":
        openmano_cli.ns_delete(args.uuid)

    elif args.command == "scenario-deploy":
        openmano_cli.ns_instantiate(args.scenario_name, args.instance_name)

    elif args.command == "instance-scenario-delete":
        openmano_cli.ns_terminate(args.instance_name)

    elif args.command == "datacenter-list":
        for uuid, name in openmano_cli.datacenter_list():
            print("{} {}".format(uuid, name))

    else:
        logger.error("Unknown command: %s", args.command)
        sys.exit(1)

if __name__ == "__main__":
    main()
