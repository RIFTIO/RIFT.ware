#! /usr/bin/env python

#
# 
	 (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
	
#

# examples:
#
# ./copyright-helper.py  --source-dir=/localdisk/nhudson/rift-nuke/rift/ --insert-placeholder=True --find-other-license=True --output-dir=./tmp-nuke
#
#
# ./copyright-helper.py --source-dir=/localdisk/nhudson/RIFT-5416/source/rift --insert-license=TRUE --output-dir=./tmp-nuke
#
# # insert license on a source-dir !automated! with minimal output (good for scripts)
# ./scripts/packaging/copyright-helper.py --source-dir /localdisk/nhudson/spot_debug_rpm_201/.install/rpmbuild/SOURCES/riftware-launchpad-4.0.2.0 --insert-license=True --interactive=false --skipchecks=True --quiet=true
#  
#

__author__ = 'Nate Hudson'
__version__= 0.6


from os import walk
import os
import re
import datetime
import time
import sys
import argparse



# pseudo-code 
#
# loop through sdir directories
#	check if dir in exclude dirs 
#
#	if not, loop through all files
#		check if ascii filetype
#			search file contents for phs and phe
#			if not found, output filelist broken down by module???
#				search for other license text
#  			if replace==true, do the replacement to the proper text
#
#



# pretty term colors
class bcolors:
    HEADER = '\033[95m'
    HEADER2 = '\033[96m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'



#DEBUG=0;
#DEBUG=1;


def printMe(p):

	#global args

	if args.quiet == False:
		print p


def init(args):

	global placeholders_start, placeholders_end, other_license_text, real_license_text
	global exclude_dirs, exclude_files, today, exclude_simple
	global DEBUG, FILE_COUNT, FILE_COUNT_HAS_BOTH, FILE_COUNT_ONLY_START, FILE_COUNT_ONLY_END, FILE_COUNT_HAS_NONE, EDICT, MDICT, BFDICT, OLDICT, NUM_REPLACED, NUM_INSERTED

	# RIFT.io placeholders start and end
	placeholders_start = [ 	'
	 (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
	', 
							'
	 (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
	', 
							'
	 (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
	', 
							'
	 (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
	', 
							'
	 (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
	', 
						]

	placeholders_end   = [ 	'',
							'',
							'' 
						]

	if args.skipchecks == False:

		# any text in dir names to exclude = NOT EXACT MATCHES
		exclude_dirs = [ 		'.AppleDouble',
						'.build',
						'.git',
						'.idea',
						'.install',
						'1.bundle.js','bundle.js','1.bundle.js.map','bundle.js.map','main.js','main.js.map','1.main.js.map', # we can't insert copyrights on these packed bundles as it will introduce syntax errors from the linebreaks
						'setup/skel', # very old, per JLM
						'modules/app', # removed in OSM flat git
						'modules/enablement', # removed in OSM flat git
						'modules/docs', # removed in OSM flat git
						'modules/host', # removed in OSM flat git
						'modules/pristine', # removed in OSM flat git
						'modules/ext/intel-qat', # removed in OSM flat git
						'modules/ext/nwepc', # removed in OSM flat git
						'modules/ext/openssl-async', # removed in OSM flat git
						'modules/ext/seagull', # removed in OSM flat git
						'modules/ext/vnfs', # removed in OSM flat git
						'modules/core/fpath', # removed in OSM flat git
						'modules/automation/systemtest', # removed in OSM flat git
					]

		# any text in file names to exclude = NOT EXACT MATCHES
		exclude_files = [		'.Parent',
						'.git',
						'.gitignore',
						'.gitmodules.deps',
						'.gitmodules',
						'id_grunt',
						'.pydevproject',
						'.project',
						'.cproject',
						'.cpack-workaround',
						'.DS_Store',
						'.tmpnew'
						'RPM-GPG-KEY',
						'foss.txt', # I don't think we need the license in the foss.txt, this ain't Inception
					]
	else:
		# when --skipchecks=true = we don't care about the exclude files and dirs as we assume the source dir is clean already
		exclude_dirs = []
		exclude_files = ['1.bundle.js','bundle.js','1.bundle.js.map','bundle.js.map','main.js','main.js.map'] # we can't insert copyrights on these packed bundles as it will introduce syntax errors from the linebreaks


	exclude_simple = []


	# search for these terms to find OTHER (pre-existing) license texts in files
	other_license_text = [ 'license', 'copyright' ]

	# came from /net/boson/home1/mharper/license/COPYRIGHT_NOTICE
	# current version of the license text
	#real_license_text = open('licenses/riftio-copyright-notice-20151211.txt').read()
	real_license_text = open( os.path.dirname(os.path.realpath(__file__))+'/licenses/riftio-copyright-notice-20151218.txt').read()

	# init
	FILE_COUNT=0
	FILE_COUNT_HAS_BOTH=0
	FILE_COUNT_ONLY_START=0
	FILE_COUNT_ONLY_END=0
	FILE_COUNT_HAS_NONE=0

	NUM_INSERTED=0
	NUM_REPLACED=0

	# EDICT is for Extensions
	# MDICT is for Modules
	# BFDICT is for Bad File lists
	# OLDICT (Other License) is used to store files w/o rift license but with another license text match
	EDICT = {}
	MDICT = {}
	BFDICT = {}
	OLDICT = {}
	OLDICT['FILES'] = []

	today = datetime.datetime.today()

        if args.debug == True:
		DEBUG=1
	else:
		DEBUG=0

	return True


def parse_directory(directory):
    """ Validate the parameter is a valid directory

    Arguments:
       	directory - a directory path

    Raises:
       	argparse.ArgumentTypeError if argument is not a directory
    """
    if not os.path.isdir(directory):
        raise argparse.ArgumentTypeError("{} is not a valid directory".format(directory))

    return directory



# takes in a path and searches for all foss.txt files and then check_fosss them
def find_check_fosses( path ):

	printMe (bcolors.WARNING+'[find_check_fosses] Searching for foss.txt files in '+path+' '+bcolors.ENDC);

	for dirName, subdirList, fileList in os.walk(path):

		for fname in fileList:
			path = os.path.join(dirName, fname)
			#printMe fname

			# skip the stupid .AppleDouble dirs
			if path.find(".AppleDouble") != -1: continue

			if fname == 'foss.txt': 
				printMe (bcolors.HEADER2+'Found foss @ '+path+bcolors.ENDC)
				check_foss(path);


	return True





# takes in a path to a foss.txt and checks if the relative FOSS directories in it exist
# and adds it to the list of excluded paths
def check_foss( foss ):

	import csv

	printMe (bcolors.WARNING+'\tChecking FOSS format of '+foss+' '+bcolors.ENDC)

	with open(foss) as csvfile:
		reader = csv.DictReader(csvfile, fieldnames=("module", "dir", "desc", "license", "url") )

		for row in reader:
			#printMe (row['dir'], row['license'])
			#printMe ('row='+str(row) );

			# allow comments with #
			if row['module'][0:1] == "#":
				continue

			module = row['module'].strip()
			dir = row['dir'].strip()
			desc = row['desc'].strip()
			license = row['license'].strip()
			url = row['url'].strip()

			printMe ('\t('+module+') dir='+dir+' has license '+license+'. desc='+desc+' url='+url+' ');

			base = foss.replace('foss.txt','')
			shouldPath = base+dir;

			simplePath = shouldPath.replace(os.getcwd(),'')			
			#print "cwd="+str(os.getcwd());
			#print "simplePath="+str(simplePath);
			#time.sleep(3)

			if os.path.exists(shouldPath):
				printMe (bcolors.OKGREEN+'\t\t+ '+shouldPath+' exists '+bcolors.ENDC)
				# add to list of excluded paths ?!?
				exclude_dirs.append(shouldPath)
				exclude_simple.append(simplePath)
			else:
				printMe (bcolors.FAIL+'\t\t- '+shouldPath+" doesn't exist "+bcolors.ENDC)

	return True



def do_insert_license(args,path):

	global placeholders_start, placeholders_end, other_license_text, real_license_text
	global exclude_dirs, exclude_files
	global DEBUG, FILE_COUNT, FILE_COUNT_HAS_BOTH, FILE_COUNT_ONLY_START, FILE_COUNT_ONLY_END, FILE_COUNT_HAS_NONE, EDICT, MDICT, BFDICT, OLDICT, NUM_REPLACED


	printMe ('\t\t[do_insert_license] path='+path)
	fn2, ext = os.path.splitext(path)
	pathstart, filename = os.path.split(path)

	# pseudo-code
	#
	# read in file
	# loop through all placeholder starts
	# search for placeholder

	if len(real_license_text) < 10:
		printMe (bcolors.FAIL+'No Real License Text Found! '+bcolors.ENDC)
		return False;


	###########################################################################################################
	# pseudo-code TO REPLACE placeholder
	#
	# just remove the 2x ends:  
	# swap out the 
/*
 * 
	 (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
	
 */

 one with just a 
	 (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
	
	# determine the PREFIX like '# ' or '* '
	# 
	# use the prefix and then 
	# replace ALL VERSIONS of placeholders to the license text
	# make sure to replace the entire line so we get don't leave a trailing hash in: # 
	 (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
	
	#

	text = ''

	try:
		# get text of source file
		text = open(path,'rb').read();

	except IOError:
		printMe ('\tUnable to read '+str(path)+' ! Maybe it is a symlink?')

	#printMe '\ntext_before:\n'+text[:300]+'\n\n'
	beforeLines = text[:300]

 	# replace some known problem placeholders
 	# !order is important!

 	# double slash comment line first
 	text = text.replace('
/*
 * 
	 (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
	
 */

','\n/*\n * 
	 (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
	\n */\n\n')
 	# then standard one line C-stle
 	text = text.replace('
/*
 * 
	 (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
	
 */

','\n/*\n * 
	 (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
	\n */\n\n')
 	# then the double hash at start and end
 	text = text.replace('# 
	 (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
	','# 
	 (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
	')

	prefix = ''

	has_start = False
	for phs in placeholders_start:
		found = text.find(phs)
		if found >= 0:
			
			# might not always work? get prefix to a comment from -2 character index of placeholder
			prefix = text[found-2]

			# add a space on to C style prefix
			if prefix == '*': 
				prefix = ' *'

			# newline prefix when there is no space between prefix and placeholder
			if prefix == '\n':
				printMe ('\t\t\t[real_license] found newline prefix ')
				#prefix = '# '
				prefix = text[found-1]

			printMe ('\t\t\t[real_license] has_start@'+str(found)+' prefix=|'+str(prefix)+'| ')
			has_start = True
	scolor = bcolors.FAIL if has_start==False else bcolors.OKGREEN

	has_end = False
	for phe in placeholders_end:
		if text.find(phe) >= 0:

			# do some cleanup first of the placeholder ENDS, we don't need them
			text = text.replace(phe,'')
			has_end = True

	ecolor = bcolors.FAIL if has_end==False else bcolors.OKGREEN

	middleLines = text[:300]

	# add on prefix character to real license
	license_text_with_prefix = real_license_text.replace('\n','\n'+prefix)

	printMe (scolor+'\t\t\t[real_license] has_start='+str(has_start)+bcolors.ENDC);
	printMe (ecolor+'\t\t\t[real_license] had_end='+str(has_end)+bcolors.ENDC);

	for phs in placeholders_start:

		# find prefix of the placeholder = comment character for source type

		# replace all starts with real license text!
		text = text.replace(phs,license_text_with_prefix)
		
	afterLines = text[:300]	
	#printMe '\ntext_after:\n'+text[:800]+'\n\n'

	do_replace = False

	if args.interactive == True:

		print ('\n'+bcolors.WARNING+'---> '+path+bcolors.ENDC)

		print (bcolors.OKBLUE)
		print ("--BEFORE-------------------------------------------------------------------------------------------------------------------------")
		print (beforeLines)
		print ("--BEFORE-------------------------------------------------------------------------------------------------------------------------")
		print ( bcolors.ENDC+" "+bcolors.HEADER+" ")
		if DEBUG: print ( "--MIDDLE-------------------------------------------------------------------------------------------------------------------------")
		if DEBUG: print ( middleLines)
		if DEBUG: print ( "--MIDDLE-------------------------------------------------------------------------------------------------------------------------")
		print ( bcolors.ENDC+" "+bcolors.HEADER2)
		print ( "--AFTER-------------------------------------------------------------------------------------------------------------------------")
		print ( afterLines)
		print ( "--AFTER-------------------------------------------------------------------------------------------------------------------------")
		print ( bcolors.ENDC)


		yn = raw_input('Make this replacement? [y/n/q] : ')

		if yn.lower() == 'y':
			do_replace = True
		elif yn.lower() == 'q':
			printMe ( "\nQuitting.\n")
			sys.exit()

	else:
		# set to true everytime, this is dangerous
		do_replace = True
		#pass


	if do_replace == True:

		printMe ( '\t\t\t\t'+bcolors.OKGREEN+'Making the replacement!'+bcolors.ENDC)

	
		try:	
			# then write out
			#new = path+'.tmpnew';
			#f = open(new, 'w')
			f = open(path, 'w')
			f.write(text)
			f.close()
			#time.sleep(5)

			NUM_REPLACED = NUM_REPLACED+1

		except IOError:
			print ('Unable to write to '+str(path)+' ')

		#printMe bcolors.HEADER2+'[TEMP] wrote out .tmpnew to: '+new+' '+bcolors.ENDC


	#if path == '/localdisk/nhudson/RIFT-5416/source/rift/modules/automation/core/util/test_module_wrapper.py':
	#	time.sleep(10)	



def do_insert_placeholder(args,path):

	global NUM_INSERTED

	fn2, ext = os.path.splitext(path)
	pathstart, filename = os.path.split(path)

	printMe ( '\t\t[do_insert_placeholder] ext='+str(ext)+' path='+path)

	do_replace = False

	beforeLines = ''
	afterLines = ''

	# only run on extensions we have rules for ?!?
	# .yang
	# .plugin
	# CMakeLists.txt
	# .cmake
	# .vala ???
	# 
	# NO.EXT like lots in scripts/setup/scripts/ (check for shebangs???)
	#
	# .xml
	# .html
	# .js
	# .jxs
	# .css
	# .scss
	# .less
	#

	#if ext == '.sh' or ext == '.py':
	if ext == '.c' or ext == '.h' or ext == '.cpp' or ext == '.hpp' or ext == '.sh' or ext == '.py' or filename == 'CMakeLists.txt' or ext == '.yang' or ext == '.orig' or ext == '.cmake':

		printMe ( '\t\t\t'+bcolors.OKBLUE+'Found a '+ext+', inserting placeholder! '+bcolors.ENDC)

		# read in
		f = open(path, 'r')
		contents = f.readlines()
		f.close()

		if len(contents) < 3:
			printMe ( '\t\t\t'+bcolors.FAIL+'Not working on tiny files.'+bcolors.ENDC)
			return False



		# different file types need different logic and comment forms
		#
		# shell and python
		if ext == '.sh' or ext == '.py' or filename == 'CMakeLists.txt' or ext == '.cmake':
		#if 0:

			beforeLines = contents[0]+contents[1]+contents[2]
			#beforeLines = contents[0]+contents[1]

			# search for shebang
			hasSheBang = contents[0].find('#!');
			#printMe 'hasSheBang='+str(hasSheBang)
			if hasSheBang >= 0:
				# insert rift placeholder to line 2 with newline before and after
				contents.insert(1, '\n# 
	 (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
	\n\n')
			else:
				# no shebang, insert rift placeholder to line 0 !
				contents.insert(0, '\n# 
	 (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
	\n\n')

			afterLines = contents[0]+contents[1]+contents[2]+contents[3]

		# C style
		elif ext == '.c' or ext == '.h' or ext == '.cpp' or ext == '.hpp' or ext == '.yang' or ext == '.orig':

			beforeLines = contents[0]+contents[1]+contents[2]
			#beforeLines = contents[0]+contents[1]

			contents.insert(0, '\n/*\n * 
	 (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
	\n *\n */\n\n')

			afterLines = contents[0]+contents[1]+contents[2]+contents[3]

		if beforeLines and afterLines:

			printMe ( '\n'+bcolors.WARNING+'---> '+path+bcolors.ENDC)

			printMe ( bcolors.OKBLUE)
			printMe ( "--BEFORE-------------------------------------------------------------------------------------------------------------------------")
			printMe ( beforeLines)
			printMe ( "--BEFORE-------------------------------------------------------------------------------------------------------------------------")
			printMe ( bcolors.ENDC+" "+bcolors.HEADER2)
			printMe ( "--AFTER-------------------------------------------------------------------------------------------------------------------------")
			printMe ( afterLines)
			printMe ( "--AFTER-------------------------------------------------------------------------------------------------------------------------")
			printMe ( bcolors.ENDC)

			if args.interactive == True:

				yn = raw_input('Make this replacement? [y/n/q] : ')

				if yn.lower() == 'y':
					do_replace = True
				elif yn.lower() == 'q':
					printMe ( "\nQuitting.\n")
					sys.exit()

			else:
				# set to true everytime, this is dangerous
				do_replace = True
				#pass


	if do_replace == True:

		printMe ( '\t\t\t'+bcolors.OKGREEN+'Making the replacement!'+bcolors.ENDC)

		NUM_INSERTED = NUM_INSERTED+1
		
		# then write out
		f = open(path, 'w')
		contents = "".join(contents)
		f.write(contents)
		f.close()
		#time.sleep(5)


#def find_placeholders(sdir):
def find_placeholders(args):

	global placeholders_start, placeholders_end, other_license_text, real_license_text
	global exclude_dirs, exclude_files
	global DEBUG, FILE_COUNT, FILE_COUNT_HAS_BOTH, FILE_COUNT_ONLY_START, FILE_COUNT_ONLY_END, FILE_COUNT_HAS_NONE, EDICT, MDICT, BFDICT, OLDICT

	# walk the input directory
#	for dirName, subdirList, fileList in os.walk(sdir):
	for dirName, subdirList, fileList in os.walk(args.source_dir):

		skip_me = False

		# add a trailing slash in case there is one in foss.txt
		dirName += '/'

		# search for dirs we don't care about
		for bad in exclude_dirs:

			if DEBUG: printMe ( bcolors.WARNING+'  (searching for '+bad+' in '+dirName+')')

			find_d = dirName.find(bad)
			if DEBUG: printMe ( bcolors.WARNING+'  (find_d= '+str(find_d)+')')

			#if dirName == '/net/strange/localdisk/nhudson/rift-clean/rift/modules/ui/composer/webapp/test/uploadServer/node_modules/cors': 
				#printMe ( bcolors.WARNING+'  (searching for '+bad+' in '+dirName+')')
				#printMe ( bcolors.WARNING+'  (find_d= '+str(find_d)+')')

			if find_d >= 0:
				if DEBUG: printMe ( bcolors.FAIL+' skipping '+bad+' directory in '+dirName+' '+bcolors.ENDC)
				#printMe bcolors.FAIL+' skipping '+bad+' directory in '+dirName+' '+bcolors.ENDC;
				skip_me = True;
		
		if skip_me == True: continue;
		
		# we've found a good dir

		# get the module name to pass the buck
		m = re.search( r'(.*)/rift/modules/(.*)' , dirName, re.M|re.I)
		if m: 
			#printMe 'm.groups()= '+str(m.groups())
			modList = str(m.group(2)).split('/')
			#printMe 'modStr='+m.group(2)+' modList='+str(modList);
			#if m: mod = str(m.group(1)).split('/'); printMe 'mod='+mod+' ';
			if len(modList) == 1: theMod = modList[0]
			if len(modList) >= 2: theMod = modList[0]+'/'+modList[1]
			#if len(modList) == 3: theMod = modList[0]+'/'+modList[1]+'/'+modList[2]
		else:
			theMod = '_super';

		printMe ( bcolors.OKBLUE+'Found directory: '+dirName+'  module='+theMod+' '+bcolors.ENDC)

		if DEBUG: printMe ( 'exclude_dirs='+str(exclude_dirs)+' ')
		if DEBUG: printMe ( "fileList="+str(fileList) )

		# loop through all files in dir
		for fname in fileList:
			skip_me = False

			# search for filenames we don't care about
			for bad in exclude_files:
				find_f = fname.find(bad);
				if DEBUG: printMe ( bcolors.WARNING+'  (searching for '+bad+' in '+fname+')'+bcolors.ENDC+' find_f='+str(find_f)+bcolors.ENDC )
				if find_f >= 0:
	        			printMe ( bcolors.WARNING+'\tskipping '+bad+' in '+fname+' '+bcolors.ENDC )
	        			skip_me = True;

			if DEBUG: printMe ( 'checking skip_me='+str(skip_me) )
			if (skip_me == True): continue;
			
			# we've found a good file
			printMe ( bcolors.OKGREEN+'\tFound file #'+str(FILE_COUNT)+' : '+fname+' '+bcolors.ENDC )

			# check for ascii or binary
			textchars = bytearray({7,8,9,10,12,13,27} | set(range(0x20, 0x100)) - {0x7f})
			is_binary_string = lambda bytes: bool(bytes.translate(None, textchars))
			# http://stackoverflow.com/questions/898669/how-can-i-detect-if-a-file-is-binary-non-text-in-python

			path = os.path.join(dirName, fname)

			try:
				is_bin = is_binary_string(open(path, 'rb').read(1024))
				if DEBUG: printMe ( '\t'+path+' is_bin='+str(is_bin) )

			except IOError:
				printMe ('\tUnable to read '+str(path)+' ! Maybe it is a symlink?')


			# we've found a text file (probably), check it for license placeholders
			if is_bin == False:
				#printMe '\tchecking for license placeholders'
				FILE_COUNT = FILE_COUNT+1

				#if fname == 'foss.txt':
				#	check_foss(path);

				#printMe 'path='+str(path);

				# get file extension
				fn2, ext = os.path.splitext(path)
				
				# init ext EDICTionary to avoid key errors
				if EDICT.get(ext) == None:
					EDICT[ext] = {}
					EDICT[ext]['HAS_BOTH'] = 0;
					EDICT[ext]['HAS_NONE'] = 0;
					EDICT[ext]['ONLY_START'] = 0;
					EDICT[ext]['ONLY_END'] = 0;

				# init ext MDICTionary to avoid key errors
				if MDICT.get(theMod) == None:
					MDICT[theMod] = {}
					MDICT[theMod]['HAS_BOTH'] = 0;
					MDICT[theMod]['HAS_NONE'] = 0;
					MDICT[theMod]['ONLY_START'] = 0;
					MDICT[theMod]['ONLY_END'] = 0;

				# init the BadFile Dict to avoid key errors
				if BFDICT.get(theMod) == None:
					BFDICT[theMod] = {}
					#BFDICT[theMod]['FILES'] = {}
					BFDICT[theMod]['FILES'] = []

				try:
					# get text of file
					text = open(path,'rb').read();

		               	except IOError:
					printMe ('\tUnable to read '+str(path)+' ! Maybe it is a symlink?')

				has_start = False
				for phs in placeholders_start:
					if text.find(phs) >= 0:
						has_start = True
				scolor = bcolors.FAIL if has_start==False else bcolors.OKGREEN

				has_end = False
				for phe in placeholders_end:
					if text.find(phe) >= 0:
						has_end = True
				ecolor = bcolors.FAIL if has_end==False else bcolors.OKGREEN

				printMe ( scolor+'\t\thas_start='+str(has_start)+bcolors.ENDC )
				#printMe ecolor+'\t\thas_end='+str(has_end)+bcolors.ENDC;

				if has_start == True and has_end == True:
					FILE_COUNT_HAS_BOTH = FILE_COUNT_HAS_BOTH+1
					EDICT[ext]['HAS_BOTH'] = EDICT[ext]['HAS_BOTH']+1;				
					MDICT[theMod]['HAS_BOTH'] = MDICT[theMod]['HAS_BOTH']+1;				
				elif has_start == True and has_end == False:
					FILE_COUNT_ONLY_START = FILE_COUNT_ONLY_START+1
					EDICT[ext]['ONLY_START'] = EDICT[ext]['ONLY_START']+1;				
					MDICT[theMod]['ONLY_START'] = MDICT[theMod]['ONLY_START']+1;				
				elif has_start == False and has_end == True:
					FILE_COUNT_ONLY_END = FILE_COUNT_ONLY_END+1
					EDICT[ext]['ONLY_END'] = EDICT[ext]['ONLY_END']+1;				
					MDICT[theMod]['ONLY_END'] = MDICT[theMod]['ONLY_END']+1;				
				else:
					FILE_COUNT_HAS_NONE = FILE_COUNT_HAS_NONE+1
					EDICT[ext]['HAS_NONE'] = EDICT[ext]['HAS_NONE']+1;				
					MDICT[theMod]['HAS_NONE'] = MDICT[theMod]['HAS_NONE']+1;				
					
					# save the bad file name too
					BFDICT[theMod]['FILES'].append(path)

					# if we have NO placeholder, search for other license text
					if args.find_other_license == True:
						has_ol = False
						for ol in other_license_text:

							# we don't want to mark a file twice
							if has_ol == True: continue

							if text.lower().find(ol.lower()) >= 0:
								has_ol = True
								printMe ( bcolors.HEADER+'\t\tfound OTHER license text! '+bcolors.ENDC )
								#OLDICT[theMod]['FILES'].append(path) 
								OLDICT['FILES'].append(path) 
								#time.sleep(5)

						if has_ol == False:
							# found no license wording
							# let's try and add in the license ?!?
							if args.insert_placeholder == True:
									do_insert_placeholder(args,path)
										

				# if we have a placeholder and want to insert license
				if has_start == True and args.insert_license == True:
					do_insert_license(args,path)



	return True

def write_excludes(args,excludes):

	printMe('Write Excludes: excludes='+str(excludes))

        #file = args.output_dir+'/'+str(dstamp)+'-copyright-report.txt'
	file = 'foss-exclude-dirs.txt'

        f = open(file,'w')

	for ex in excludes:
	        f.write(ex+'\n')

	f.close()

	print( bcolors.WARNING+'Wrote Exclude Dirs to: '+str(file)+bcolors.ENDC )
	time.sleep(10)
	#sys.exit()
	

def report(args):

	global placeholders_start, placeholders_end
	global exclude_dirs, exclude_files
	global DEBUG, FILE_COUNT, FILE_COUNT_HAS_BOTH, FILE_COUNT_ONLY_START, FILE_COUNT_ONLY_END, FILE_COUNT_HAS_NONE, EDICT, MDICT, BFDICT, OLDICT
	global today

	################################### REPORT ############################################

	dstamp = today.strftime('%Y%m%d_%H%M');

	report = '\n';
	report += '\n----- RIFT Copyright Report -----'
	report += '\n\nsourceDir='+args.source_dir;

	#printMe EDICT.keys()

	report += '\n\n\n--- File Extensions ---';

	for ext in sorted(EDICT.keys()):
		report +=  '\n\nExtension: '+str(ext)

		report += '\n\t'+'HAS_ONE'+':\t'+str(EDICT[ext]['HAS_BOTH']+EDICT[ext]['ONLY_START']+EDICT[ext]['ONLY_END'])
		report += '\n\t'+'HAS_NONE'+':\t'+str(EDICT[ext]['HAS_NONE'])

		#for stat,value in EDICT[ext].items():
		#	report +=  '\n\t'+str(stat)+':\t'+str(value);

	report +=  '\n\n\n--- Modules ---';

	for mod in sorted(MDICT.keys()):
		report +=  '\n\nModules: '+str(mod)

		report += '\n\t'+'HAS_ONE'+':\t'+str(MDICT[mod]['HAS_BOTH']+MDICT[mod]['ONLY_START']+MDICT[mod]['ONLY_END'])
		report += '\n\t'+'HAS_NONE'+':\t'+str(MDICT[mod]['HAS_NONE'])

		#for stat,value in MDICT[mod].items():
		#	report +=  '\n\t'+str(stat)+':\t'+str(value);

	#report +=  sorted(BFDICT.keys());

	report +=  '\n\n'
	for mod in sorted(BFDICT.keys()):
	#if 0:
		cleanMod = mod.replace('/','-')
		if cleanMod == '': cleanMod="_super"

		if args.output_dir:
			#theFile = 'output-osm1/'+str(cleanMod)+'.txt'
			theFile = args.output_dir+'/'+str(dstamp)+'-'+str(cleanMod)+'.txt'
			report +=  '\n- Saved '+str(len( BFDICT[mod]['FILES'] ))+' files in BF list to: '+str(theFile);
			theOutFile = open(theFile, 'w') 

			for item in sorted(BFDICT[mod]['FILES']):
				# remove full path
				cleanItem = item.replace(args.source_dir,'')
				theOutFile.write("%s\n" % cleanItem)

	if args.output_dir:
		theFile = args.output_dir+'/'+str(dstamp)+'-other_license_files.txt'
		report +=  '\n\n\n- Saved '+str(len( OLDICT['FILES'] ))+' files in OtherLicense list to: '+str(theFile);
		theOutFile = open(theFile, 'w') 

		for item in sorted(OLDICT['FILES']):
			# remove full path
			#cleanItem = item.replace(args.source_dir,'')
			#theOutFile.write("%s\n" % cleanItem)
			theOutFile.write("%s\n" % item)


	FILE_COUNT_HAS_AT_LEAST_ONE = FILE_COUNT_HAS_BOTH+FILE_COUNT_ONLY_START+FILE_COUNT_ONLY_END

	if FILE_COUNT > 0:
		m1 = float(FILE_COUNT_HAS_AT_LEAST_ONE) / float(FILE_COUNT)
		percent = round( (m1*100) ,2)
		#report += '\n\n'+'m1='+str(m1)+' m2='+str(m2)+' m3='+str(m3)+'\n\n'
	else:
		percent = float(0.00);

	report +=  '\n\n\n--- Totals ---';
	report +=  '\n  FILE_COUNT_HAS_BOTH= '+str(FILE_COUNT_HAS_BOTH)
	report +=  '\nFILE_COUNT_ONLY_START= '+str(FILE_COUNT_ONLY_START)
	report +=  '\n  FILE_COUNT_ONLY_END= '+str(FILE_COUNT_ONLY_END)
	report +=  '\n  FILE_COUNT_HAS_NONE= '+str(FILE_COUNT_HAS_NONE)
	report +=  '\n '
	report +=  '\n           FILE_COUNT= '+str(FILE_COUNT)
	report +=  '\n '
	report +=  '\n '+str(FILE_COUNT_HAS_AT_LEAST_ONE)+' out of '+str(FILE_COUNT)+' files have at least one placeholder: '+str( percent )+'% success '

	printMe ( bcolors.OKBLUE )
	printMe ( report )
	printMe ( bcolors.ENDC )

	if args.output_dir:
		#reportFile = 'reports/copyright-report-'+str(dstamp)+'.txt'
		reportFile = args.output_dir+'/'+str(dstamp)+'-copyright-report.txt'

		rf = open(reportFile,'w')
		rf.write(report) 

		printMe ( 'Report saved to: '+reportFile )
	else:
		printMe ( bcolors.HEADER+'Specify an --output-dir to save the list of files w/o placeholders. '+bcolors.ENDC+'\n' )


	print "Worked on "+str(args.source_dir)+" "
	print "Examined "+str(FILE_COUNT)+" files."
	print "Inserted "+str(NUM_INSERTED)+" placeholders."
	print "Replaced "+str(NUM_REPLACED)+" placeholders with copyright text."

	print 'Done.'
	printMe ( '\n' )
	print ( bcolors.HEADER+'==== RIFT.io Copyright License Helper ===='+bcolors.ENDC )


# http://stackoverflow.com/questions/15008758/parsing-boolean-values-with-argparse
def str2bool(v):
  return v.lower() in ("yes", "true", "t", "1")

def parse_args(argv=sys.argv[1:]):
    """ Parse the command line arguments

    Arguments:
       	argv - The list of command line arguments (default: sys.argv)

    """
    global args

    parser = argparse.ArgumentParser()
    parser.register('type','bool',str2bool) # add type keyword to registries for proper bool arguments


    parser.add_argument('--source-dir',
                        required=True,
                        type=parse_directory,
                        help='directory to scan for foss.txt files and license placeholders')

    parser.add_argument('--output-dir',
			required=False,
                        #type=argparse.FileType(mode="w"),
                        type=parse_directory,
                        help='directory to save reports and the lists of files w/o placeholders')

    parser.add_argument('--debug',
			required=False,
                        type=str2bool,
                        default=False,
                        help='lots of debug output [Default: False]')

    parser.add_argument('--quiet',
			required=False,
                        type=str2bool,
                        default=False,
                        help='very minimal output, good for scripts [Default: False]')

    parser.add_argument('--find-other-license',
			required=False,
                        type=str2bool,
                        default=False,
                        help='search for other_license_text in files w/o rift placeholders [Default: False]')

    parser.add_argument('--insert-placeholder',
			required=False,
                        type=str2bool,
                        default=False,
                        help='actually insert the placeholder in the files w/o rift placeholders and w/o other license text !YOU MUST USE --find-other-license WITH THIS OPTION! [Default: False]')

    parser.add_argument('--insert-license',
			required=False,
                        type=str2bool,
                        default=False,
                        help='searches for rift placeholders, and replaces with actual license text. for ex. when making distribution tarballs [Default: False]')

    parser.add_argument('--interactive',
			required=False,
                        type=str2bool,
                        default=True,
                        help='do you want a human to review every replacement? [Default: True]')

    parser.add_argument('--skipchecks',
			required=False,
                        type=str2bool,
                        default=False,
                        help='if skipchecks is enabled, the script skips all foss checks and ignore/exclude rules as it assumes the source_dir is cleaned already (like with a RPM build) [Default: False]')

    parser.add_argument('--saveexcludes',
			required=False,
                        type=str2bool,
                        default=False,
                        help='do you want to save the exclude_dir paths to a file? [Default: False]')

    #parser.add_argument('--foss-files',
    #                    nargs="+",
    #                    type=argparse.FileType('r'),
    #                    help='extra foss text file paths')

    args = parser.parse_args(args=argv)


    return args



def main():

	global args;

	print ( bcolors.HEADER+'==== RIFT.io Copyright License Helper ===='+bcolors.ENDC )
	args = parse_args()

	init(args)

	if args.skipchecks == False:
		# find all foss.txt and load them into the exclude list
		find_check_fosses(args.source_dir)

		# should be long after we find them
		printMe ( '\nexclude_dirs='+str(exclude_dirs)+'\n' )
		#time.sleep(5);

		#printMe ( '\nexclude_simple='+str(exclude_simple)+'\n' )
		#time.sleep(20);

	if args.saveexcludes == True:
		write_excludes(args,exclude_simple)

	# find all the placeholders in the dir
	#find_placeholders(args.source_dir)
	find_placeholders(args)

	# output the report
	report(args)




if __name__ == "__main__":
    global args;
    main()















