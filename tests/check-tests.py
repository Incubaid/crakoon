#!/usr/bin/env python

import os
import os.path
import time
import shutil
import logging
import tempfile
import subprocess
import contextlib

try:
    import cStringIO as StringIO
except ImportError:
    import StringIO

PATH = os.getcwd()
NAME = os.environ.get('ARAKOON_EXECUTABLE_NAME', 'arakoon.native')
EXECUTABLE = os.path.abspath(os.path.join(os.getcwd(), NAME))
BASE_PORT = 5000

if not os.path.isfile(EXECUTABLE):
    raise RuntimeError('Arakoon not found at %s' % EXECUTABLE)

if not os.access(EXECUTABLE, os.X_OK):
    raise RuntimeError('Arakoon binary not executable at %s' % EXECUTABLE)

@contextlib.contextmanager
def arakoon_cluster(n):
    nodes = [
        ('crakoon_%d' % i, '127.0.0.1', BASE_PORT + (2 * i),
        BASE_PORT + (2 * i + 1))
        for i in xrange(n)
    ]
    base = tempfile.mkdtemp(prefix='crakoon_test')
    cluster = 'crakoon'

    logging.info('Running inside %s', base)

    config = StringIO.StringIO()

    config.write('''
[global]
cluster = %s
cluster_id = %s
''' % (', '.join(node[0] for node in nodes), cluster))

    for node in nodes:
        config.write('''
[%(NAME)s]
ip = %(IP)s
client_port = %(CLIENT_PORT)d
messaging_port = %(MESSAGING_PORT)d
home = %(BASE)s/%(NAME)s
log_level = info
''' % {
    'NAME': node[0],
    'IP': node[1],
    'CLIENT_PORT': node[2],
    'MESSAGING_PORT': node[3],
    'BASE': base,
    })

        os.mkdir(os.path.join(base, node[0]))

    config_path = os.path.join(base, 'config.ini')

    logging.debug('Writing config to %s', config_path)
    with open(config_path, 'w') as fd:
        fd.write(config.getvalue())

    procs = set()

    for node in nodes:
        name = node[0]

        args = [EXECUTABLE,
                '-config', config_path,
                '--node', name,
                ]

        logging.info('Launching node %s', name)
        proc = subprocess.Popen(
            args, executable=EXECUTABLE, close_fds=True, cwd=base)

        procs.add(proc)

    logging.info('Waiting for nodes to settle...')
    time.sleep(5)

    try:
        yield cluster, nodes
    finally:
        for proc in procs:
            logging.info('Killing proc %d', proc.pid)
            proc.kill()

        logging.info('Removing tree %s', base)
        shutil.rmtree(base)

def test_arakoon_client():
    # Run against a single node for now, since the testcase itself doesn't
    # handle master switches
    with arakoon_cluster(1) as (cluster, nodes):
        args = [os.path.join(PATH, 'test-arakoon-client')]

        args.append(cluster)

        for node in nodes:
            args.append(node[0])
            args.append(node[1])
            args.append(str(node[2]))

        logging.info('Running "%s"', ' '.join(args))
        subprocess.check_call(args)

def main():
    test_arakoon_client()

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    main()
