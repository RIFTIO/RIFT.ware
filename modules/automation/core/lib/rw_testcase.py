"""
#
# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
#
# @file rw_testcase.py
# @author Jeremy Mordkoff (jeremy.mordkoff@riftio.com)
# @date 11/12/14
# @brief Testcase management
# @details This module contains classes which can be used to schedule, cancel and query
# the testcase manager portion of the test harness
#
"""

import logging
import os
import json
import requests
import re

logger = logging.getLogger(__name__)

class InvalidConfiguration(Exception):
    pass

class APIFailure(Exception):
    pass

class LocalTestCaseManager(object):
    ''' LocalTestCaseManager provides a local implementation of the testcase manager system

    The server has testcases and jobs
    testcases are identified by the path to the config file relative to RIFT_ROOT
    a job has a workspace and a testcase which implies a config file
    '''

    return_codes = [ 'PASS', 'FAILED', 'DEFERRED', 'EXCEPTION', 'TIMEDOUT', 'ABORTED']

    status_codes = {    'Q': 'Queued',
                        'R': 'Running',
                        'C': 'Complete',
                        'S': 'Started',
                        'A': 'Aborted',
                    }
    '''
    0 PASS -- the test ran to the end and passed
    1 FAILED -- the test ran to the end but failed
    2 DEFERRED - the test exited during setup because prerequisites were not met (vm not available, etc)
    3 EXCEPTION - the test driver or test raised an exception
    4 TIMEDOUT -- the driver killed the test because it took too long to run
    5 ABORTED -- the harness or driver killed the test because of user intervention (control-c) or a system shutdown
    '''

    def __init__(self, **kwargs):
        logger.info("Local TestCaseManager created.")
        self.job_id = self._gen_job_id()
        self.queue = []

    @classmethod
    def get_code(cls, err):
        return cls.return_codes.index(err)

    def get_status(self, code):
        return self.status_codes.get(code,"")

    def _gen_job_id(self):
        _id = 0
        while 1:
            yield _id
            _id += 1

    def submit_job(self, config_file, rift_root=None, priority=100):
        job = {
            'workspace': rift_root,
            'config_file': config_file,
            'priority': priority,
            'submitted_by': os.getenv('USER', 'None'),
            'status': 'Q',
            'id':next(self.job_id),
        }
        logger.debug("submitted %s in %s priority %d" % ( config_file, rift_root, priority ))
        self.queue.append(job)

    def submit_log(self, rift_root, config_file, logfile, result):
        job_params = {
                'submitted_by': os.getenv('USER', 'None'),
                'workspace': rift_root,
                'config_file': config_file,
                'status': 'C',
                'logfile_path': logfile,
                'result': self.return_codes[result],
        }
        self.update(job_params)

    def get_queue(self, username=None):
        return [job for job in self.queue if job['status'] == 'Q']

    def update(self, job_params):
        jobs = [index for index, job in enumerate(self.queue) if job.config_file == job_params.config_file]
        if not jobs:
            logger.debug("Failed to identify job to be updated")
            return

        if 'result' in job_params and isinstance( job_params['result'], ( int, long )):
            job_params['result'] = self.return_codes[job_params['result']]

        queue_index = jobs[0]
        job = self.queue[queue_index]
        self.queue[queue_index].update(job_params)


class TestCaseManager(object):
    ''' TestCaseManager implements the interface to the testcase manager system

    The server has testcases and jobs
    testcases are identified by the path to the config file relative to RIFT_ROOT
    a job has a workspace and a testcase which implies a config file
    '''

    return_codes = [ 'PASS', 'FAILED', 'DEFERRED', 'EXCEPTION', 'TIMEDOUT', 'ABORTED']

    status_codes = {    'Q': 'Queued',
                        'R': 'Running',
                        'C': 'Complete',
                        'S': 'Started',
                        'A': 'Aborted',
                    }
    '''
    0 PASS -- the test ran to the end and passed
    1 FAILED -- the test ran to the end but failed
    2 DEFERRED - the test exited during setup because prerequisites were not met (vm not available, etc)
    3 EXCEPTION - the test driver or test raised an exception
    4 TIMEDOUT -- the driver killed the test because it took too long to run
    5 ABORTED -- the harness or driver killed the test because of user intervention (control-c) or a system shutdown
    '''


    def __init__(self, **kwargs):
        url = os.getenv('TESTCASE_URL', 'http://reservation.eng.riftio.com:80')
        if 'url' in kwargs:
            url = kwargs['url']
        self.urls = {
                      'testcase': url + "/testcase/REST/testcases/",
                      'job':     url + "/testcase/REST/jobs/",
                      'jobfilter':     url + "/testcase/REST/jobs/filter/",
                    }
        self._updates = []
        logger.info("TestCaseManager created, url is %s" % url)

    def __del__(self):
        self._put_updates()
        for update in self._updates:
            logger.warning("UPDATE FAILED: %s" % update)

    @classmethod
    def get_code(cls, err):
        return cls.return_codes.index(err)

    def get_status(self, code):
        return self.status_codes.get(code,"")

    def _post(self, typ, data):
        ''' post a request

        args:
            data -- dictionary of parameters
            typ -- object type -- 'testcase' or 'job'

        returns
            dictionary of the results

        convert data from a dictionary to json
        post it
        if it fails, raise APIFailure
        else return the results as a dictionary
        '''
        # @TYPE r: request
        url = self.urls[typ]
        return self._post_url(url, data)

    def _post_url(self, url, data):
        js = json.dumps(data)
        r = requests.post(url, data=js, headers={'content-type': 'application/json'})
        if r.status_code < 200 or r.status_code > 299:
            #import pdb; pdb.set_trace()
            raise APIFailure("post %s data %s error %d" % (url, js, r.status_code ))
        return r.json()

    def _put_url(self, url, data):
        js = json.dumps(data)
        r = requests.put(url, data=js, headers={'content-type': 'application/json'})
        if r.status_code < 200 or r.status_code > 299:
            #import pdb; pdb.set_trace()
            raise APIFailure("post %s data %s error %d" % (url, js, r.status_code ))
        return r.json()


    def _get(self, typ, data):
        url = self.urls[typ]
        r = requests.get(url, params=data)
        if r.status_code < 200 or r.status_code > 299:
            raise APIFailure("get %s error %d" % (url, r.status_code ))
        return r.json()


    def submit_job(self, config_file, rift_root=None, priority=100 ):
        ''' submit a new job to the testcase server. This job will be executed by
            the test harness daemon (if any)

            config_file is the path to the racfg file relative to rift_root
            priority is higher runs sooner
        '''
        if rift_root is None:
            rift_root = os.getenv('RIFT_ROOT', None)
        if rift_root is None:
            raise InvalidConfiguration("RIFT_ROOT was not specified not found in the environment")
        params = {
                  'workspace': rift_root,
                  'config_file': config_file,
                  'priority': priority,
                  'submitted_by': os.getenv('USER', 'None'),
                   }

        logger.debug("submitted %s in %s priority %d" % ( config_file, rift_root, priority ))
        self._post('job', params)

    def submit_log(self, rift_root, config_file, logfile, result ):
        ''' create a record on the testcase server to hold the test result
            config_file is the racfg path relative to rift_root
            logfile is the absolute path
            result is numeric -- see comments around line 45
        '''
        param = { 'submitted_by': os.getenv('USER', 'None'),
                  'workspace': rift_root,
                  'config_file': config_file,
                  'status': 'C',
                  'logfile_path': logfile,
                  'result': self.return_codes[result],
        }
        self._post('job', param)

    def get_queue(self, username=None):
        ''' retrieve the list of jobs that need to be executed from the testcase server
        '''
        self._put_updates()
        params = { 'status': 'Q' }
        if username:
            params.update({"username": username})

        res = self._get('jobfilter', params )

        return res

    def update(self, job):
        ''' update the test case server with the status of the job
        '''
        param = dict()
        for p in ['id', 'workspace', 'config_file', 'status', 'priority', 'submitted_by', 'result', 'host', 'pid', 'logfile_path' ]:
            if p in job:
                param[p] = job[p]

        if 'result' in param and isinstance( param['result'], ( int, long )):
            param['result'] = self.return_codes[param['result']]
        self._updates.append(param)
        self._put_updates()

    def _put_updates(self):
        failed_updates = []
        for update in self._updates:
            url = "%s%d/" % ( self.urls['job'], update['id'] )
            try:
                self._put_url(url, update)
            except Exception as e:
                logger.warning("_put_updates: %s" % e)
                failed_updates.append(update)
        self._updates = failed_updates
        if len(self._updates):
            logger.info("WARNING: %d failed updates" % len(self._updates))


def main():
    tc = TestCaseManager(url=os.environ.get('TESTCASE_SERVER', 'http://tau:8888'))
    '''
    please see the harness script for an example as to how to submit jobs
    '''
    for job in tc.get_queue():
        print("job %d submitted by %s priority %d workspace %s test %s" % ( job['id'], job['submitted_by'], job['priority'], job['workspace'], job['config_file']))


if __name__ == '__main__':
    main()


