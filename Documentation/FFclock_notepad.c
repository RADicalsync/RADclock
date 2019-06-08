/*
 *  kernel_summary_notepad.c
 *  
 *
 *  Created by Darryl Veitch on 2/03/15.
 *
 */

/* Free-format summary for FFCLOCK kernel code and needed elements */

/* Kernel version studied here:  is kernel/freebsd/FreeBSD-CURRENT/  from
   Programs/V4/Git_readonly/radclock  checkout, later copied to
	Programs/V4/RADclock_mycurrent    (though some strange (c) --> (C) conversions)
  radclock code:  was unchanged except including some late changes found in davis's files,
                  regarding moving the rdb_mutex into a queue structure etc (see radclock summary).
 
  kernel code: This version was targetting FreeBSD-9.3 though it was never labelled as such.
				   However, these directories contain patches. The actual files are collected under
 
     Programs/BSDKernel/Platypus_FreeBSD9.0_RADClock0.4/modifiedFiles_FreeBSD9.3
 
  as flat files,so named as they were supplied by Vijay who claimed they were for 9.3 .
  For the moment the policy is to not alter these in any way.
  
  [ Note that the same files, but in the correct directory structure and with some early attempts at documentation and Vijay's labelling of FF components by 'FFCLK' (see mail of March 13, 2015) (replaced shortly after by this file), are found in
  Programs/BSDKernel/Platypus_FreeBSD9.0_RADClock0.4/NewlySavedfromemail/modifiedFiles ]
 
  Differences (very few) with the online 10.1 version are mentioned below.
  Note however that the most substantial differences are expected in the bpf code, which
  is absent from 10.1 .
  
  Additional summary points from Vijay email trail:
    - 10.1 :   [ email of March 20, 2015]
	     - using KV=2, but 9.3 patched kernel uses KV=3, and seems more advanced in other ways
		  - has independent updates though in kern_tc.c
		  - no FFclock content in bpf.{h,c}, if_{em,igb}.c
	 - 9.0 release on Platypus was KV=3:  kern_ffclock.c:static int ffclock_version = 3'
	 - silvereye was an attempt to get 10.1 working, it is therefore KV=2 following the KV of 10.1, but what bpf changes were put in?   KV 2 or 3?
 
*/
	

/************************** General *****************************/

Comments
 - more is good.  Big picture/context blocks can be put into man pages (9?)
 - big blocks at head of fns are fine, can add input arg comments
 - dont spell out anything obvious to a good programmer, like C tricks
 - can use tools unidef, unidefall (standard in BSD under /usr/local) for stripping 
   out based on some #define label
 - footnotes are bad

Lawrence
 - particularly knowledgeable on syscall and bpf parts
 - recommends Warner Losh as a reviewer, as he is now at netflix with him!
 - maybe find a way to make timing inclusion important for netflix
 - short paper good, also BSD magazine and FreeBSD publicity
 - check man pages for documentation, also the BSD?? manual? for timecounter?
 - knows a lot about mercurial, a reason for using it..
 - uses DTRACE a lot!

Version control/committing
 - FreeBSD is svn based, copies clones to github periodically
 - mercurial: more intuitive   
   >hb-beta command..    // hb is the chemical symbol for mercury
 - git:   gives direct access to github - good publicity
 - Etiquette for changing code maintained by others:
    - contact them and ask for review of patches
	- to find them, use  svn blame  http:/svn.freebsd.../..   /file.c
	                     or "hg annotate" in mercurial or "git blame" 
 

Tools for navigating code
 - fxr.watson.org   [ also exists for linux, flr? ]
 - powerful terminal version (integrates with editor): cscope .
   - if installed, do "make scope" in sys/
 - look at community threads in lists.freebsd.org  
   ** Old thread I looked at the time:  freebsd source committers r227778

 
Tools for building
 - options in sys/conf/options 
   -- careful, these ALways exist as in "such an option exists", not in #ifdef sense
   -- get auto generation of "opt_fflock.h" IF (I think) have #ifdef FFCLOCK
 - look at man pages
 - make.conf controls default parameters passed to make
 - faster compiling:
	-- use switch NO_CLEAN, only compiles parts that have changed
	-- another risker method for further speedup, called?
	-- workflow:  high risk, ifnotwork, NO_CLEAN, else full compile

Tools for debugging
 - DRACE very useful (better than KTRACE) can do conditional stuff
 - use KASSERT to generate a crash conditional on an "invariant" something that
   should be true, so if it isnt, need to be urgently alerted!
   When normal building, such "invariant checking" is switched off
 - use panic() to conditionally panic even without invariant checking, used when
   continuing would cause damage, like loss of data.
 - debug modes:
	realtime: tricky to use (is this kgdb?)
	postcrash: inspect crash dump file.
	-- must ensure crash dump saving  (on by default in Head)
	-- ensure have adequate swap: put  dumpdev="AUTO" in /etc/defaults/rc.conf 
	   memory dump is a vmcore file. Load it (using kgdb?) and kernel that crashed
	   (usually in boot/boot/?) to start debug session
 - workflow: 	put in KASSERT, let it crash, do crashdump analysis	   


/***************** Milestone Planning *****************/
Immediate TODO:
   FF: status codes,  -ve delta
   RAD: extra NTPtime fixes
   	  add Docs to git
   	  Fix 2, 3 off the summary bug list nextBUG:
        plocal confusions...

Phase 1:   Codebase recovery  [ getting My work back under My control ]

 Level 0:		Announcing to CAIDA that it is coming
 	- Completion of naive porting
		- rough documentation and understanding of 9.0-->11.2 changes
		- insertion of trivial improvements and my reversions
		- compile, simple runnning and a priori working
		- requesting their use case and target arch and OS version

 Level 1:		Establishment of new FreeBSD11.2-RADclockV5 based version for all future internal work
   Meta-point here is that the version u have been using for ages has either has the
   issues below (RADclock side) or has unknown issues (FF), so dont be too fussy!! already
   doing Better than before, as well as with more confidence !  graduate fast to this.

	- investigation and correction of long-term known issues
		- pkt matching    [DONE]
		- identical key insertion [DONE]  nonces now must be strictly mono-inc (if counter is)
		- queueing effect due to multi-instance ts of UDP packets
		  - now know this is a serious FF issue involving bpf modifs and snapshot tech
		  	  - dont wait for FF perfection! that is for Level4. As long as it seem to work,
		    proceed to testing, that will help understanding for next FF steps and enable
		    RAD to proceed even if FF not perfect (will get to see if naive ports do harm)
		- fix FF<-->RAD mode debacle, make simple and explicit
			- if get KV complexity, then do explicit KV switching, dont make KV=3,4 suffer
			  through pointless backcompat.
		- broken leap second code screwing stuff up  [DONE, but check]
	- fix daemon-side tsmode/mode debacle, and associated bugs. Ditto on avoid KV=3.4 complexity
	- Fixing and clarification of known BUGS and `bugs` from summaries
	- fix defn of Ca across kernel/daemon and correct leapsec structure [DONE?]
	- checking plocal switchon/off mechanism and default to phat if off
		- check restart change support  (for change in counter or anything else )
	- decisions on key points
		- valid_till field	[DONE]
		- daemon side CaMono     [ not algo, is about value-added reading ]
			 - make SHM hold past params, enabling simple mono within library read fns?
			 - clear understanding needed here, but no implementation reqd for Level-1
		- a priori meaningful error reporting  (not ideal, just meaningful, self-consistent) [DONE?]
	- working trigger retransmission code   [DONE]
	- working (and not too onerous?)  TO adaptation code in TRIGGER [Done until non-VM testing)
	- ensure conf settings allow the  daemond sync of sysclock to be switched off independently from other things  [DONE]
	- re-git  to enable Yi testing, and NTC deployment, dev and testing with Yi [done]
	- running test SHM experiments
		- test 4-tuple with SHManalyze
		- viewNTP analysis of algo basics
		- long term SHM (on useful server list) to test rarer errors, python matching issues.
	- nothing wierd in log you cant explain



 Level 2:		Delivery of FreeBSD11.2-RADclockV5 `stable` version to CAIDA
 	- more detailed checks on algo perf
 	- beginning on algo Doc to help check everything
 	- more detailed checks on config options and code branches
	- checks on various timestamping options
		- libprocesses
		- sysclock=FFclock
		- timestamping calls and error estn fns  [ if they care ]
	- ensure bypass works if they want TSC, and counter selection in general
	- package creation !
	- customization
		- tailored read fn?
		- sysclock=FF ?
		- use RADclock server so it is an accurate S2 ?
		- use RADclock spy to get an S1.5 ? benchmark it !
	- RADclock server
		- basically working
		- not dangerous
		- uses diff clock to get Te from Tb



 Level 3:		Ready to contact FBSD & Laurence to negotiate FF version4 patches
 	- Metapoint is that FF+sysclock and bpf different !
		- FF+sysclock in fact already done, that is why it is already in there!
			- so need to add version4 value
			- need to my name in older version fairly
			- add matching corrected daemon so that it can actually be used and tested
	 	- bpf stuff separate
			- FF timestamping
			- timestamp splitting [efficiency]
			- per-bpf_if  separation, and ABI implications
			- link to credible deamon even more important
 	- all version4 changes I want inserted and tested (via custom RADclock verbosity + direct)
		- inclues boottime logic and leapsecond logic
	- understanding of bpf_if stuff clear
	- bypass stuff sorted
	- readiness to produce and offer patches
	- having used each of Lawrences suggested workflows at least once
	- familiarity with FreeBSD svn submission interface, authorship aspects
	- familiarity with git, using git repos internally
	- having gone through old commit messages, absorbed history, hints
	- understand relevant licenses
	- be ready to discuss authorship headers ...
	- TODOs in kernel Doc done


 Level 4:		FF patches accepted
 Level 5:		Key NTC functionality
 						- multiple sources/RADclocks
 						- whitelisted clients
 						- server clock decision algo
 						- ∆uA tracking algo
 Level 5:		Xen functionality restored





Jpap points:
	- MD? markup  way to add code easily into a doc ```
	- publish on github
	- mount mac dir on VM using samba,  then only one copy of files, automatically backed up .


 
/**o*************** History Notes *************/
2019
Mar 6
	Copied the documented version to V4 to become the working CURRENT :
	cp ../BSDKernel/Platypus_FreeBSD9.0_RADClock0.4/modifiedFiles_FreeBSD9.3 /* FFclock_mycurrent */
Mar 8
	Snapshotted my radclock code before the big push initiated by the 11.2 port :
	cp -r RADclock_mycurrent RADclock_beforeporting_11.2
March 9,10
	first merging of my first fixes, with examination of 11.2 new stuff to check existing port
	timeeffc.h:  		no port change, just added my #define MS_IN_BINFRAC  etc
	kern_fflock.c:		port changes limited to neater sysctl_kern_sysclock_active, I slightly reverted
							added in use of #define MS_IN_BINFRAC   and comments
March 22  [after return from Google Summit]
	kern_tc.c			many port changes, described below
							kept all, minor reversions to ..
							added in use of #define MS_IN_BINFRAC   and comments


/***************** Xcode Cheat Sheet *****************/

CMD 0  :  toggle Project Navigator  [ dir/file list on left ]
CMD /  :  toggle commenting of highlighted text



/***************** System Cheat Sheet *****************/
sysctl -a | grep clock    // catches all the new sysctl tree entries, can see KV !
sysctl -a | grep time

### On VM

#### System
pkg {install,remove} xfce

#### Communicating with Sharp
scp -r OriginalSource_11.2 darryl@172.19.115.80:Papers/TSCclock/Programs/V4 // once only

#### FFclock
cp PatchedSource_11.2/bpf.{c,h} /usr/src/sys/net		// copy to source dir
cp PatchedSource_11.2/kern_ffclock.c /usr/src/sys/kern/

diff PatchedSource_11.2/kern_ffclock.c 	/usr/src/sys/kern/kern_ffclock.c
cp PatchedSource_11.2/kern_ffclock.c	/usr/src/sys/kern/

make -j2 -DNO_CLEAN buildworld
make -j2 -DNO_CLEAN buildkernel KERNCONF=FFCLOCK
make installkernel KERNCONF=FFCLOCK
reboot

make installworld
mergemaster -Ui
reboot

#### RADclock
cd radclock_install_directory
autoreconf -if  		// regenerate configure is needed (eg if make fails)
./configure 			// not ioctl=0 in current configure.in,  needs looking at
make
make install

radclock -x -p 1   -t ntp1.net.monash.edu.au -D 5000  -n tom  -vv  -w routine_run.raw

less  /var/log/radclock.log
grep ffc /var/log/messages

grep -v Debug radclock.log | less
cat radclock.log | grep attemp



### On MAC
v4
scp PatchedSource_11.2/bpf.*  root@172.16.20.129:FFkernel_Work/Sharp_filedrop // will overwrite
scp -r RADclock_mycurrent  root@172.16.20.129:FFkernel_Work/Sharp_filedrop

scp -r RADclock_mycurrent/radclock/pthread_dataproc.c   root@172.16.20.129:FFkernel_Work/RADclock_mycurrent/radclock/

scp -r RADclock_mycurrent/radclock/pthread_dataproc.c   root@172.16.20.129:FFkernel_Work/Sharp_filedrop

scp -r RADclock_mycurrent/libradclock/pcap-fbsd.c   root@172.16.20.129:FFkernel_Work/Sharp_filedrop
scp    FFclock_mycurrent/kern_ffclock.c   			 root@172.16.20.129:FFkernel_Work/PatchedSource_11.2



/***************** Git Cheat Sheet ***********************/
###### Git
git clone ssh://darryl@cubinlab/home/git/radclock/clock-analysis.git    % creates new dir in current
git clone ssh://darryl@cubinlab/home/git/reports/ISPCS2012_host_asym.git/

git status
git branch                  % see what branches you have
git ls-files                % see what files have been committed
git log                     % viewing log of your commits with a graph to show the changes
git diff                    % look at diffs of all modified files compared to last commit
git diff HEAD^              % compare with commit before that (I think..)

%% Creating and merging branches
git checkout master         % puts current branch in drawer and pulls out master from the compressed repository
git checkout -b mybranch master   % this creates a new branch, copied off master
git push origin mybranch    % the origin will be set to emu
                all
git merge mybranch
git commit
git push

%% Normal work cycle
git checkout master         % just pulls out the master `draw' to work on, you already have it
git add file.tex            % add file to staging area  (this assembles a complex tailored commit)
git commit -m 'My comment'  % commits everything in staging area, ie, makes a local snapshot of it
git pull origin mybranch    % must get the latest before pushing your latest.  Use correct branch!
git push origin mybranch    % pushes the commit to the central repository. If branch not master, must specify


%% Julien recommended workflow when working on local branch, but ultimately pushing to remote master
%% This model avoids trying to modify the central repository and potentially finding conflicts, instead,
%% bring the latest to you, and resolve it all locally, then enjoy a problem free push in confidence.
%% This workflow is equivalent to one based on rebasing, but it keeps the local master as a pristine
%% (out of date in general) copy of the central one, (handy) keeping the changes in a separate branch.
%% Unnecessary, but neat, cautious. The alternative is to stage the changes, then rebase to the central master.  The disadvantage, I guess, it that if you have a problem, or want to go back, you dont have the same separate branch playing field ready for you, you would somehow have to backpedal and cleanup first.
%% Ahh acutally not equivalent, here you would still be merging, merge will look back in time to see the common ancestory, then reapply all commits in some order, it will not apply the branch commits on top of the updated master, only rebase does that.  So this workflow is perhaps a waste of time.

% setup
git checkout -b darryl master   % create new local branch, assuming already have local copy of master
% cycle
git add, commit             % work on local branch, no need to pull, noone else is working on it..
                            % don't push! that is only for pushing to remote master
git checkout master         % pull out local copy of master
git pull                    % sync to central master to get latest changes of others before trying to merge
git merge darryl            % merge darryl into the current branch (which is the local master)
                            % resolve conflicts if needed
git push                    % push clean version to central master in repository
git checkout darryl         % go back to darryl
git merge master            % bring darryl up do date.
                            % must work, since master has already successfully merged in darryl changes
                            % alternative would be to delete darryl, then recreate from master

% git reset						% reset staging area to match most recent commit (no change to WDir)
% git reset file				% remove from staging area only (dont change it in the WorkingDir)
% git reset --hard			% reset SA (Staging Area) and WD back to that of most recent commit
% git reset --hard <commit> % go back to this commit & its SA, kill (orphan) all commits after it

%% Central repository workflow
Yi:
 
#I have set up the git server in the broadway and create a bare repository in the /home/git/RADClock.git and a symlink in the soft/RADClock.git.
I only enable  SSH for Git, no web interface is available.
As we use 2233 for SSH, you need to specify the port number when clone and push.
Here is an example of using git account, the password is the same as the soft account.
 
#git clone ssh://git@138.25.60.68:2233/home/soft/RADClock.git
#git remote add RADClock ssh://soft@138.25.60.68:2233/home/soft/RADClock.git
#git push RADClock master
 


// Create the repo in pre-existing dir
cd /Users/darryl/Papers/TSCclock/Programs/V4/RADclock_mycurrent
git init 		// create a new, empty init in current dir
git config --global user.email "Darryl.Veitch@uts.edu.au"
git config --global user.name "Darryl Veitch"

// connect to remote central repo
git remote add RADclockCentral  ssh://git@138.25.60.68:2233/home/soft/RADclock.git
git remote -v
git fetch --dry-run RADclockCentral
checkout RADclockCentral			// have a look to confirm nothing there
checkout master

// add the entire existing project and push it to the central repo
git add -v --dry-run .		// add the whole current dir tree
git add .
git commit -m "Initializing master with private, definitive, RADclock_mycurrent dev dir."
git log --oneline
git tag -a V4-restart-dev -m "In development RADclock v4 compatible with in development FFclock v3"
git tag
git push -v --dry-run RADclockCentral
git push -v 	RADclockCentral 	V4-restart-dev

// Looking in the bare git repo to see what is there
logb
cd ../soft/RADclock.git
git show							// see all objects, and if a dir (tree) or file (blob)
git ls-tree HEAD				// see all top level objects, and if a dir (tree) or file (blob)
git ls-tree fb0c				// show object with ID starting with this

// Create my dev branch
git checkout -b  V4dev  master    // create new branch with initial state the same as master
git branch -v

// Examples on dev
# Renaming: do via git to ensure history preserved, will add to staging area immediately
# could do via the shell, and git should autodetect at add or commit time, but time delay, and may fail
git mv radclock/pthread_dataproc.c radclock/pthread_dataproc.c



/***************** Compiling Cheat Sheet *****************/

# Lessons
 - snapshots
 	- regularly, also clean out regularly
	- do just before any compile, after all files uploaded, make attempted, useful listings on screen
	- time to save other workflow work also:  term setting, cheat sheet, scripts..
 - compilation approach
 	- try in hierarchy of:  kern only with NOclean, then without, then world NOclean, then world
	- need to learn when a waste of time though, currently, only extremes seem to work..
	- don't forget needed install and reboot steps!  and to remake the daemon for the new kernel
 - current versions of source
 	- built up library of better techniques for efficiency
	- next steps: 
		- Mount all of V4 under FFclock_work, so have all in one hit
		- adapt the move script to pull out directly, make script bash !!
		- create new minimum RADclock_install dir, and experiment with 
	   	  MINimum inside that works: starting with {lib}radclock, then inputs to configure
		- then add script to push new/all .{c,h} there  and make etc there	  



# Kernels:

The following procedures are the example of enabling FFCLOCK in the amd64,
other architectures follows the same procedures in their own subdirectory.

		  1) cd /usr/src/sys/amd64/conf
        2) cp GENERIC MYKERNEKNAME       		// eg FFCLOCKKV3
        3) Edit the MYKERNEKNAME file as follows:
              include GENERIC                  # include the GENERIC kernel
              ident   MYKERNEKNAME             # name this kernel  (will see in uname -a )
              options FFCLOCK                  # enable FFCLOCK components
              options PPS_SYNC                 # enable the PPS
        4) Compile   // world elements are heavy, only do if change bpf.{c,h}
				  cd /usr/src
				  make -j2 -DNO_CLEAN buildworld		// if needed
				  make -j2 -DNO_CLEAN buildkernel KERNCONF=MYKERNELNAME
				  make installkernel KERNCONF=MYKERNELNAME
		  5) Reboot
	 // If needed:
		  6) Fix up world
				  cd /usr/src
			     make installworld
				  mergemaster -Ui 					// merges old and new to enable apps
		  7) Reboot


# RADclock

cd radclock_install_directory
autoreconf -if  		// regenerate configure is needed (eg if make fails)
./configure 			// not ioctl=0 in current configure.in,  needs looking at
make
make install

// If aclocal problematic, try to automake using ports:
	portsnap fetch extract update
	cd /usr/ports/devel/automake
	make install clean



/***************** Note on FFcode port observed in 11.2 *****************/


timeeffc.h:  		no changes
kern_fflock.c:		port changes limited to neater sysctl_kern_sysclock_active
						I slightly reverted by removing goto, want to remove continue as well
kern_tc.c			Many changes.  Main topics are
	 # bootime veriable management
	 	- new member timehands.th_boottime  Makes good sense, each tick has an associated value
		  for conversion to UTC, otherwise, global variables  boottime{bin} may be out of sync with tick
	 	- bunch of new globals to support this, in bt and timeval variants
		- tc_windup(struct bintime *new_boottimebin);  // now pass value to use for new tick
		- bootime value access now always mediated by functions  getboottime{bin} instead
		  of just reading globals, or recalculated per-tick values using these.  Used in key places:
		    sysctl_kern_boottime  tc_setclock  sysclock_snap2bintime
	 	 Nice idea, but does this mean no standard interface to actually learn the current
		  bootime?  Must always use the current tick?   so could be giving a time which is
		  wrt UTC before a leap occuring during a tick?
		 Are the globals ever even used now? even in tc_setclock they are not
	 	- getboottimebin(struct bintime *boottimebin_x)
		    - simply returns th_boottime member of current timehands safely
		- getboottime(struct timeval *boottime_x)
			 - just uses getboottimebin  then   bintime2timeval

	 # access control in timehands   [ in VM, try man atomic_load_acq_int ]
	 	- use of fences to provide protection for the read and use of gen variables themselves in gen-check sysctl_kern_ffclock_ffcounter_bypass
		- used in
			- all references to {ff}timehands gen-check loops
		   - some limited explanation in tc_windup
	 	- structure in the load/reading-protected code is:
				gen = atomic_load_acq_int(&th->th_generation);	// force this atomic read to be completed
						  // before any other accesses occuring later wrt source-order
				execute work (not involving gen)
				atomic_thread_fence_acq();			// force prior loads to complete before any subsequent L or S
		  I guess needed since, if have to go round loop again, need to ensure that the final values you exit with are the right ones?
	 	- structure in the store/setting-protected code is:     [ used in tc_windup ]
						th->th_generation = 0;
						atomic_thread_fence_rel();	// force prior loads AND stores to complete b4 subsequent S
						....
					   ogen = 1;
						atomic_store_rel_int(&th->th_generation, ogen);
	 	- lots of use in the pps systems..


		Summary:
			_acq_  Aquire  semantics, relates only to a load (read) from memory, says must do it `now`
					  	- makes sense when value may change later by some other thread, need to grab before this
			_rel_  Release semantics, relates only to a store (write) to memory, says prior loads or stores
			       must be complete first
					   - makes sense when about to change value, so operations needing the old one better finish
		   fence:  provides the bookend of a acq or rel to insist on relevant ordering inbetween
			      acq ... fence_acq :  two-way barrier for load  :  get it, operate, all up to date, continue
			      fence_rel ... rel :  two-way barrier for store : get up to date, operate, ensure stuff since fence done first to get up to date again



	 # other timehands changes
	 	- new convenience UTC member  th_bintime
			- more code, but more efficient {get}bintime(), as bootimebin adding now done once-only per tick
	 	- change from 10 to 2, no explanation, did they misunderstand and though it was only about
		  protection?  not late processing of requests?
		  Code still all seems to be generic for arbitrary number
	 	- does the fencing ensure that code will complete before updating?  but this could slow
		  tick advancement!  need discussion with author
	   - Maybe, Given that you use fence for safety reasons, then not even possible to use more than 2 anymore, so delay risk the price of safety?
	 	- better self-doc init of th0
		- th_generation no longer volatile,  I guess only timehands needs to be, I spotted that myself
		- globals time_{second,uptime}  now volatile, dont know why - track usage


	 # changes in fftimehands  and  ffclock_windup
	 	- removed member:  	struct bintime		tick_error;		  // bound in error for tick_time
			- and related update code in  ffclock_windup
			- perhaps because was seen to be never used?
			  or error should be reported elsewhere in analogous way to FB error?
	 	- ffclock_windup and ffclock_change_tc  benefit from new fence protection in tc_windup
			- no FF related changed in tc_windup
	   - protection in ffclock_read_counter

	 # sysclock_getsnapshot logic changed !  
	 	- since tick_error removed, had to do full calc here.  Is inefficient, for same
		  reason convenience tick-start cals where precalculated for many other members !


	 # more control to changed tc
		- more globals to support more checks/controls on tc
			- SYSCTL_NODE _kern_timecounter_tc_  is now static
				- callback is sysctl_kern_timecounter_adjprecision, calls tc_adjprecision
				  - uses new globals  {s}bt_{time,tick}threshold   tc_{precexp,timepercentage}
		   - inittimecounter
			    - uses new globals   tc_tick_{s}bt
				 - also uses tc_adjprecision  Seems to be all about being much more careful when
				   iniializing to get initial view of tc right, but very technical, no F(*&^ comments !
			- volative int rtc_generation = 1;
			- static int tc_chosen;	/* Non-zero if a specific tc was chosen via sysctl. */
			  - used in tc_init  to skip the automatic adoption of a new counter (if quality sufficient)
			  after initialization.  Set in sysctl_kern_timecounter_hardware
		   - tc_setclock
					- smarter mutexing and other stuff
		   - tc_ticktock or inittimecounter
			  	- when tc_windup called, gets passed NULL, not the boottime
				- called with bootime only within  tc_setclock
		   - tc_cpu_ticks
				- some kind of increased safety using some critical_enter()  technology


	 # pps upgrades  [ lots of stuff here ]
	 	- some abi aware stuff that could be useful to learn for bpf
		- new stuff under  PPS_SYNC,  whatever that is
		- no changes to FFCLOCK parts



	 # other:  
	 	- includes:
				lock.h and mutex.h always included, not FFclock protected
				limit.h included, provides:
				param, queue, sdt  :  needed for an SDT Probe  (Statically Defined Tracing from DTrace)
		- void dtrace_getnanotime(struct timespec *tsp);  // clone of getnanotime immune from fbt probing
				- used as a timestamping function callable by dtrace, see cddl/dev/dtrace/arch../dtrace)subr.c
		- style change:   inclosing return values in ( )
		- vdso stuff (quite a bit of it)  what is it??
		- sbuf-ication of  sysctl_kern_timecounter_choice








/***************** Bits from, and about, .h files *****************/


Sharp> llt *.h   // exclude linux
// automatically generated
-rw-r--r--@ 1 darryl  admin   16663 26 Feb 14:36:57 2015 freebsd32_syscall.h
-rw-r--r--@ 1 darryl  admin   12141 26 Feb 14:36:58 2015 syscall.h
-rw-r--r--@ 1 darryl  admin   59971 26 Feb 14:36:57 2015 freebsd32_proto.h
-rw-r--r--@ 1 darryl  admin  135451 26 Feb 14:36:58 2015 sysproto.h

// Existing files with FFclock modifications
-rw-r--r--@ 1 darryl  admin   36286 26 Feb 14:36:58 2015 bpf.h
?/?/timepps.h    // Where did I get it below?   is on watson, all versions
                 // not included in modifiedFiles_FreeBSD9.3
                  // vijay .doc says is in  head/sys/sys/timepps.h
  - replaced by libradclock/radclock-timepps.h?  but this would not be used by kernel

// New pure FF files
-rw-r--r--  1 darryl  admin    1835 26 Feb 14:36:58 2015 _ffcounter.h
-rw-r--r--  1 darryl  admin   12170 26 Feb 14:36:58 2015 timeffc.h

/* Other important .h with no FFclock modifications */




/********* sys/sys/freebsd32_syscall.h *********/
Automatically generated.
#define	FREEBSD32_SYS_ffclock_getcounter	241
#define	FREEBSD32_SYS_ffclock_setestimate	242
#define	FREEBSD32_SYS_ffclock_getestimate	243

/********* sys/sys/syscall.h *********/
Automatically generated.
#define	SYS_ffclock_getcounter	241
#define	SYS_ffclock_setestimate	242
#define	SYS_ffclock_getestimate	243

/********* sys/sys/freebsd32_proto.h *********/
Automatically generated.
#include <sys/_ffcounter.h>

/********* sys/sys/sysproto.h *********/
Automatically generated.     // grep sysproto.h ffc
#include <sys/_ffcounter.h>
Included details of syscall prototypes, including structures, syscalls, arguments




/********* sys/sys/_ffcounter.h *********/
- contains only:
#ifndef _SYS__FFCOUNTER_H_
#define _SYS__FFCOUNTER_H_
typedef uint64_t ffcounter;
#endif /* _SYS__FFCOUNTER_H_ */
 - included by
	timeffc.h
	timepps.h 
	bpf.h
 - is unprotected by FFCLOCK
 - compare with the library version from radclock.h :
   typedef uint64_t vcounter_t;
	- different name just to avoid confusion? or to ensure FF stuff and RADclock independent?
   - this means that both types will be defined in libprocess code



/********* timeffc.h ***********/
- included in   kern_{tc,ffclock}.c, subr_rtc.c, bpf.c, ffclock.2
- includes _ffcounter.h, so no need for above files to include it directly
- note it has no ifdef FFCLOCK in it, all things here always defined


/*
 * Feed-forward clock estimate
 * Holds time mark as a ffcounter and conversion to bintime based on current
 * timecounter period and offset estimate passed by the synchronization daemon.
 * Provides time of last daemon update, clock status and bound on error.
 */
struct ffclock_estimate {
	struct bintime	update_time;	/* Time of last estimates update. */
	ffcounter	update_ffcount;	/* Counter value at last update. */  previously "TSClast"
	ffcounter	leapsec_next;	/* Counter value of next leap second. */ my idea of a counter based trigger
	uint64_t	period;		/* Estimate of counter period. */
	uint32_t	errb_abs;	/* Bound on absolute clock error [ns]. */
	uint32_t	errb_rate;	/* Bound on counter rate error [ps/s]. */
	uint32_t	status;		/* Clock status. */
	int16_t		leapsec_total;	/* All leap seconds seen so far. */
	int8_t		leapsec;	/* Next leap second (in {-1,0,1}). */
};

- unlike timehands which keeps track of what the counter and clock says at the last 
  interrupt cycle tick, this is related to the algos updates, ie, this is basically the kernels copy of the (FF abstracted) algo parameters, plus:
	-- the absolute time for convenience, UTC I guess - to be confirmed that this is traditional
	   Ca, which is UTC without subsequent leapseconds, and without LERp!
	-- leapsecond stuff, which is needed for a complete clock state. But if leap here,
	   does this mean it is absent in the daemon?  isnt in radclock_data but is in
		handle->algo_output.  Who is setting/owning it? is this structure used to push or pull it?
	-- note no lerp stuff here - not part of the underlying FF clock
- name is poor, imprecise and doesnt give purpose, I guess he was desperate to avoid
  `global data' Perhaps ffclock_data (by analogy with radclock_data) or ffclock_parameters
- what is the units of period?  ahh look in ffclock_convert_delta, seems to be a fraction
  of a second in 64bit binary (frac component of bintime format)
- no plocal here- presumably period is set to plocal in the RADclock specific update code..
  this is generic.  Ditto for setting the origin and incorporating drift correctly- 
  Need to check that absolute time updates include all RADclock components in the right way..
- leap second Convention: a "positive" leap second means add
  an extra second, displayed on UTC clocks as  23:59:60, before passing to 00:00:00 .
  This is a delay added, the clock is made to run slower, it is set Back.
  Negative (never yet used) means passing from 23:59:58 to 00:00:00.


The above include and defn always defined, the rest of the file protected by
#if __BSD_VISIBLE
#ifdef _KERNEL

- Hint from Laurence (I think):  checks like #if __BSD_VISIBLE, #ifdef _KERNEL are
(at least in some cases) due to portability. Eg if reused elsewhere and needed
to ensure POSIX compliance, need to screen out parts which may not be.
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%



- index into sysclocks array seems to suggest only two kinds, rather than many
more, each of FF or FB type..?    similarly sysclock_snap only supports a preferred
FF and FB clock at most, not all currently available.  Check this makes sense, but
certainly seems enough for now.  Where specify which daemon to use?
  - make sure you are not confusing this will different tc choices !

#define	FFCLOCK_SKM_SCALE	1024
  - not syncd with the daemons versionis a simple fallback kernel only copy
  - used only once in   kern_tc.c:ffclock_windup :  if (bt.sec > 2 * FFCLOCK_SKM_SCALE)
  	  to test if daemon date very too stale to be trusted

#define	FFCLOCK_STA_UNSYNC	1
#define	FFCLOCK_STA_WARMUP	2
  - how do these compare with the daemon status words?  many more of those, why just these?

#define	FFCLOCK_{FAST,LERP,LEAPSEC,UPTIME,MASK}
#define	FBCLOCK_{FAST,             UPTIME,MASK}
 - flags to support traditional kernel reading variants and metainfo
 - useful comments


// Clock info structures designed to support quick snapshot and comparison
// of FF and FB clocks and more
struct fbclock_info {
	struct bintime		error;
	struct bintime		tick_time;
	uint64_t		th_scale;
	int			status;
};
struct ffclock_info {
	struct bintime		error;
	struct bintime		tick_time;         // definition?
	struct bintime		tick_time_lerp;
	uint64_t		period;
	uint64_t		period_lerp;
	int			leapsec_adjustment;
	int			status;
};
struct sysclock_snap {
	struct fbclock_info	fb_info;
	struct ffclock_info	ff_info;
	ffcounter		ffcount;
	unsigned int		delta;
	int			sysclock_active;
};

void ffclock_reset_clock(struct timespec *ts);
void ffclock_read_counter(ffcounter *ffcount);    // THE way to read counter?
void ffclock_last_tick(ffcounter *ffcount, struct bintime *bt, uint32_t flags);



// FF/FB clock selection
#define	SYSCLOCK_FBCK	0
#define	SYSCLOCK_FFWD	1
extern int sysclock_active;   //  declared here, defined in kern_tc
  - records if taking a FF or FB clock, can use to index functions, ascii name..

// FFclock reading functions                                    		defined in
	  counter reading       ffclock_read_counter			  		    		kern_tc.c
	  low level FF:			ffclock_convert_{abs,diff,delta}    		kern_tc.c
	  general FF workhorse:	ffclock_{abs,diff}time				    		kern_ffclock.c
	  FF specialized:		ffclock_[get]{bin,nano,micro}[up]time   		kern_ffclock.c
	  FB specialized:		fbclock_[get]{bin,nano,micro}[up]time   		kern_tc.c
	    (same as old             [get]{bin,nano,micro}[up]time
		public:						  [get]{bin,nano,micro}[up]time_fromclock  timeffc.h
   All of these always declared, but NONE defined unless FFCLOCK is, except old names 
   and *_fromclock

Public _fromclock Functions
- declared as static inline, which means whenever
timeffc.h is included, these "functions" are expanded like a #define, but with
type checking, so that are not really new fns, so ok to define them in a .h file. 
Since they are static, any such inclusion/expansion is sealed from any other, 
avoiding any confusing about multiple definitions.
- there is no extra protection or checking, benefit is just that naming is analogous
  to the old functions, just with a _fromclock extension.  Still, for experts only anyway.



/* Feed-Forward Clock system calls. */
__BEGIN_DECLS         // why do you need this?
int ffclock_getcounter(ffcounter *ffcount);
int ffclock_getestimate(struct ffclock_estimate *cest);
int ffclock_setestimate(struct ffclock_estimate *cest);
__END_DECLS





/********* timepps.h ***********/
- has many structures and variables with FF components, ALWAys defined, eg
	136 struct pps_state {
	137         /* Capture information. */
	138         struct timehands *capth;
	139         struct fftimehands *capffth;  // breaks old interfaces, binaries?
	140         unsigned        capgen;
	141         unsigned        capcount;
	142 
	143         /* State information. */
	144         pps_params_t    ppsparam;
	145         pps_info_t      ppsinfo;
	146         pps_info_ffc_t  ppsinfo_ffc;  // and here
	147         int             kcmode;
	148         int             ppscap;
	149         struct timecounter *ppstc;
	150         unsigned        ppscount[3];
	151 };
- architecture seems to be:  augment structures, but write new 
  analogous functions if actually want to act on new FF parts.
- note this approach not taken with timehands..

										














/***************** Bits from, and about, .c files *****************/

// automatically generated
-rw-r--r--@ 1 darryl  admin   62999 26 Feb 14:36:57 2015 init_sysent.c
-rw-r--r--@ 1 darryl  admin  134437 26 Feb 14:36:57 2015 freebsd32_systrace_args.c
-rw-r--r--@ 1 darryl  admin   63614 26 Feb 14:36:57 2015 freebsd32_sysent.c
-rw-r--r--@ 1 darryl  admin   23517 26 Feb 14:36:57 2015 freebsd32_syscalls.c
-rw-r--r--@ 1 darryl  admin  139404 26 Feb 14:36:58 2015 systrace_args.c
-rw-r--r--@ 1 darryl  admin   20631 26 Feb 14:36:58 2015 syscalls.c

// Existing files with FFclock additions
-rw-r--r--@ 1 darryl  admin    5674 26 Feb 14:36:57 2015 subr_rtc.c
-rw-r--r--@ 1 darryl  admin   62922  7 Dec 12:28:21 2016 bpf.c
-rw-r--r--@ 1 darryl  admin  163339 26 Feb 14:36:57 2015 if_igb.c   ??
-rw-r--r--@ 1 darryl  admin  165212 26 Feb 14:36:57 2015 if_em.c
-rw-r--r--@ 1 darryl  admin   47275 11 Apr 00:04:55 2015 kern_tc.c

// New pure FF files
-rw-r--r--@ 1 darryl  admin   12810  7 Apr 14:27:40 2015 kern_ffclock.c

// File that are there but apparently with no FF additions
-rw-r--r--@ 1 darryl  admin   32121 26 Feb 14:36:57 2015 kern_ntptime.c

Also files options has
FFCLOCK
and NOTES has
options 			FFCLOCK




/********* if_igb.c ***********/
#include <net/bpf.h>			// can't find any difference, and no Julien in header

...
static void igb_configure_queues(struct adapter *adapter)



/********* if_em.c ***********/
#include <net/bpf.h>

============================ NEW summary
Drivers need modifications to use the new bpf functions, and to ensure timestamping
occurs in the right place.

dev/e1000/if_em.c		// changes built on 9.0.0, quite a few other changes in 9.2, 10
  Unprotected, but "\\FFCLOCK"  comment flagging FF changes
  Changes revolve around inclusion of call to ETHER_BPF_MTAP at end of em_xmit
  	 - requires additional 3rd argument  to em_xmit( , , struct ifnet *);
	 - call  ETHER_BPF_MTAP
  Resulting changes:
      em_xmit						: new defn,  moving call to ETHER_BPF_MTAP inside
		em_{,mq}_start_locked   : call to em_xmit,  commenting out call to ETHER_BPF_MTAP
  Other:
  		em_initialize_receive_unit	:
	   // Disable any type of interrupt throttling
	   //	E1000_WRITE_REG(hw, E1000_ITR, DEFAULT_ITR);
	   E1000_WRITE_REG(hw, E1000_ITR, 0);
		DTRACE stuff flagged by \\ben, exactly as for if_igb


dev/e1000/if_igb.c	 // changes built on 9.0.0   File missing from watson
  Unprotected, but "\\ben"  comment flagging FF changes, as well as DTRACE defines and a probe defn.
  Exactly analogous changed to if_em.c, just applied to igb_xmit instead of em_xmit .
  Other:
		igb_rxeof					: just adds a DTRACE probe, not related to FF
		  SDT_PROBE(timestamp, if_igb, igb_rxeof, stamp, sendmp, 0, 0, 0, 0);





================================
// FFCLOCK
//static int	em_xmit(struct tx_ring *, struct mbuf **);
 - commented out..

static em_vendor_info_t em_vendor_info_array[] =

- lots of function prototypes

static int em_probe(device_t dev)
static int em_attach(device_t dev)
static int em_detach(device_t dev)
static int em_shutdown(device_t dev)
static int em_suspend(device_t dev)
static int em_resume(device_t dev)

#ifdef EM_MULTIQUEUE
static int
em_mq_start_locked(struct ifnet *ifp, struct tx_ring *txr, struct mbuf *m)
// FFCLOCK
//		if ((err = em_xmit(txr, &next)) != 0) {
//		ETHER_BPF_MTAP(ifp, next);
  - commented out..

static int em_mq_start(struct ifnet *ifp, struct mbuf *m)
static void em_qflush(struct ifnet *ifp)
static void em_start_locked(struct ifnet *ifp, struct tx_ring *txr)
// FFCLOCK
// To be able to call BPF_MTAP from em_xmit
//		if (em_xmit(txr, &m_head)) {

// FFCLOCK
// This call after em_xmit() does not preserve causality
// If called before em_xmit() (which can fail) takes the risk to send 2 copies of the
// same packet. Need to move it into em_xmit()
//		ETHER_BPF_MTAP(ifp, m_head);


static int em_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
static void em_init_locked(struct adapter *adapter)
...
// FFCLOCK
//static int
MOdified fn with 3rd param:
static int em_xmit(struct tx_ring *txr, struct mbuf **m_headp, struct ifnet *ifp)
..

Modification seems to be only at end to use passed ifp  :
// FFCLOCK
// This cannot fail
		ETHER_BPF_MTAP(ifp, m_head);

...
static void em_initialize_receive_unit(struct adapter *adapter)
..
// FFCLOCK
// Disable any type of interrupt throttling
//	E1000_WRITE_REG(hw, E1000_ITR, DEFAULT_ITR);
	E1000_WRITE_REG(hw, E1000_ITR, 0);





/********* kern_ntptime.c ***********/
#include <sys/timepps.h>        // can't find any difference, look into patch !
- this is the NTPd API so to speak, for user and daemon programs
- has PLL and FLL parameters
- some interpolation stuff here that may be useful/inspire FFclock routines
- has a  time_second variable, but not the same as the global in kern_tc .



static int ntp_is_time_error(void)

static void ntp_gettime1(struct ntptimeval *ntvp)

int sys_ntp_gettime(struct thread *td, struct ntp_gettime_args *uap)
- calls ntp_gettime1 with mutex(&Giant) protection

static int ntp_sysctl(SYSCTL_HANDLER_ARGS)
- again calls ntp_gettime1, not a sysctl.. ??

- PPS stuff

int sys_ntp_adjtime(struct thread *td, struct ntp_adjtime_args *uap)

void ntp_update_second(int64_t *adjustment, time_t *newsec)

static void ntp_init()

static void hardupdate(offset)
 - seems to be the actual algo?
 - comments about when shift from PLL to FLL

void hardpps(tsp, nsec)
 - PPS sync algo, interesting.

int sys_adjtime(struct thread *td, struct adjtime_args *uap)
int kern_adjtime(struct thread *td, struct timeval *delta, struct timeval *olddelta)
static void periodic_resettodr(void *arg __unused)
static void shutdown_resettodr(void *arg __unused, int howto __unused)
static int sysctl_resettodr_period(SYSCTL_HANDLER_ARGS)
static void start_periodic_resettodr(void *arg __unused)








/********* subr_rtc.c ***********/
This is about the Real Time Clock system, what is the connection to the system clock?

#include "opt_ffclock.h"
#ifdef FFCLOCK
#include <sys/timeffc.h>
#endif

static device_t clock_dev = NULL;
static long clock_res;
static struct timespec clock_adj;

void clock_register(device_t dev, long res)

// Initialize the time of day register   (todr)
void inittodr(time_t base)
	 #ifdef FFCLOCK
		 ffclock_reset_clock(&ts);          // only time this fn used
	 #endif
 - BUG:  should also have FF entry under the wrong_time jump point
 - could be a good entry point to learn about missing pieces in the overall system
   time picture, at least wrt the RTC.
	Look for globals:  CLOCK_GETTIME, and uts_offset()
 

void resettodr(void)

Use made of macros  CLOCK_{SET,GET}TIME(clock_dev, &ts);   defined where?








/********* kern_ffclock.c ***********/
// Rationale for inclusion here compared to kern.c:
 - must be pure FF, ie all new, and FF related
 - more workhorse stuff?  but some lower level FF stuff is in kern_tc
 - stuff that is not directly needed by `original` kern_tc stuff?
   - ie, need to modify some kern stuff with FF stuff, including some
	  pure FF functions that are directly used, so include there to have all in one place
   - but those must-be-visible FF fns rely on other underlying FF not directly used, they
	  can be hidden here.
   - FF syscalls and sysctl fall into this category, not use in kern_tc?
 - other possible rationales:
   FF code needs to support 3 things:
	   - FF counter within an existing timecounter mechanism, and basic read function
		- basics required of an FF daemon, in particular `global data` and pcap stuff
		- support for an FF based system clock (and beyond, FF,FB system clock options)
 - So:  could have:
      - kern_tc.c:   FF counter
		- kern_ffclock.c support for an FF system clock, assuming an FF counter
		      [ including daemon support, basically just syscalls ]
		- system clock:  mainly kern_tc but really both    where did the sysclock live before?
				- kern_tc  core sysclock stuff that was there before, FF-generalised
				- kern_ffclock.c  required workhorse stuff that is pure FF, so all new, and self contained
				      - eg sysclock requires lerp functionality, so FF version appear here.
		
		
		


FFCLOCK protection:
- includes timeffc.h and "opt_ffclock.h", not protected - where come from?
- otherwise entire file is protected except deliberate syscall warnings at end
  - this includes all the sysctl available clocks stuff
			  
			  
FEATURE(ffclock, "Feed-forward clock support");
 - if FEATURE a macro?  where defined? what do?
										
External variables defined in kern_tc :
54 extern struct ffclock_estimate ffclock_estimate;
55 extern struct bintime ffclock_boottime;
56 extern int8_t ffclock_updated;
57 extern struct mtx ffclock_mtx;  // so what are the threads here?



ffclock_abstime(ffcounter *ffcount, struct bintime *bt,struct bintime *error_bound, uint32_t flags)
  - workhorse fn behind the various ffclock_[get]{bin,nano,micro}[up]time wrappers
  - uses low level Ca routines (which are UTC but without any leapseconds seen since boot),
    then adds in leapseconds, and converts to uptime if required.
  - also calculates error bounds and returns counter value if "requested" via non-NULL pointer
    None of the standard wrappers use these, of course.
  - uses externs ffclock_estimate and ffclock_boottime
  - * Generation check: is it the point that bcopy is not atomic, so this is to ensure
    consistency of cest?  But why not used whenever bcopy is? hmm, no always is 
	there a danger .. look carefully each time.
	 --  The point is that, asynchronously wrt the tick updates underlying timehands and hence timecounter updates, the FFclock daemon is updating ffclock_estimate. Need to ensure dont read in middle of change
	 --  would be no problem if bcopy (in fact memcpy) were atomic, but this cant be assumed
	
  - this routine takes care of leapseconds, which the lower level routines 
    evidently ignore
	  -- flag activates:  		bt->sec -= cest.leapsec_total;     which seems to
	     - incorporate the leapsecs as required:  Eg, for +ve leaps, clock jumps back, so need to subtract from a Ca without leap correction, which is what is happening.
		  - same thing with the 	bt->sec -= cest.leapsec; if +ve, then will subtract one, which means this new leap has happened, so set clock back accordingly.
	  -- system seems to be that "leapsec"  adds in a leap that hasnt yet been included in leapsec_total, need to check code for the latter.
	  -- interaction with FFCLOCK_FAST :  if at last tick the latest leap isnt included, then
	    can include it here, that suggests  (leapsec,leaptotal) updated per-tick.
	  -- will leapsecond_next be moving around? dangerous?? can it ever move back ??
	  -- bt->sec is signed, so is safe, though since is UTC, unlikely to ever go -ve
	  -- this fn allows both FFCLOCK_LEAPSEC  and FFCLOCK_UPTIME to be applied, but for
	    UPTIME  leaps should simply be ignored completely.  Is this managed simply by ensuring that these options never called together? [apparently yes]  should we add code to prevent?
  - ** idea:  `leap has passed` seen as a kernel `state` is dangerous as Ca can be wrong, but if you make it dependent only on that you are just about to pass back, then by defn, only abs timestamps past the leap (no matter what kind of clock, mono, CA, Lerp), ever have the new leap updated.  Aah, but a single accumlated leap variable does act as a state, need to keep treat the new one differently until is it definitely accepted a safe distance past the leap.
         - maybe overthinking this, probably nothing to do with safe distance, just needed to complete per-tick management, since the leap will always occur intra-tick. To avoid doubling back, just ensure that leapsec_total setting code can NEver go back !
			
  - use of LERP flag occurs within  ffclock_{last_tick,convert_abs}.  This routine allows
    the return of either type but details not managed here.
	 
  - Error bound:
    --  why calculated here and not in the low level routines?
	    - Yes it takes time and this way u only do the work if u want it
		 - is about isolating stuff not requiring fftimehands at a higher level outside kern_tc.c
	 -- BUG:  if read live, then 		ffdelta_error = ffc - cest.update_ffcount;
        will be +ve, but if FFCLOCK_FAST, then this could be -ve .
		  Since is a bound, should add in absolute value anyway.
    -- is relative to last algo update, not HZ tick, of course, but this is 
	    an exception to the rest of the code, eg ffclock_windup.
		 Ahh but here once have ffc (which benefits from windup), then we are in the simpler FFclock world.
	 -- ** 1st constant based on 1e6, not 1e12 . Is err_bound_rate in mus/s ?
		  also, 2nd arg to bintime_mul is 64bit, unlike fn defn ?  is 32bit * 64bit
		  revisit after finding code setting cest.{errb_rate,errb_abs}
		  - ok, multiple errors.  I think this should be 1e12 here as per the comment.
		    This matched errb_rate which should be ps/s, though code BUG put ns/s instead!
		  - the absolute error is consistent:  errb_abs is in ns/s, and conversion factor
		    here cancels it with 2^64/1e9 .
	 -- TODO:  error units here, 1e-12 and 1e-9 defined in FF file kern_fflock.c;
	           must match those of cest.{errb_rate,errb_abs}, defined in FF file timeffc.h
				  but also daemon library files radapi-base.c:{fill_ffclock_estimate,fill_clock_data}.  So better to define some macro constants, but
				  where put them to ensure available to FF and to daemon? Could be in
				  timeffc.h as well as a backup in kclock.h,  to use same trick as for cest structure.
		  
	 
	 -- error calc breakdown with comments :
	 	ffdelta_error = ffc - cest.update_ffcount;    // delta since last cest update
		ffclock_convert_diff(ffdelta_error, error_bound);  // put ∆t = ∆ff * p in error_bound
		/* 18446744073709 = int(2^64/1e12), err_bound_rate in [ps/s] */
		bintime_mul(error_bound, cest.errb_rate *     // now error_bound has error in [sec]
		    (uint64_t)18446744073709LL);                // BUG should take abs value since error bound must be +ve
		/* 18446744073 = int(2^64 / 1e9), since err_abs in [ns] */
		bintime_addx(error_bound, cest.errb_abs *
		    (uint64_t)18446744073LL);
     ok tricky bit here is need to know that 2nd arg of bintime_mul is an unsigned int,
	  (not a bin), that is supposed to carry a binary fraction, hence the 2^64 factors.
	 
		  
  - only returns counter value ffc if passed a non-null pointer, currently never used,
    since this workhorse is used by functions wanting to return 'time', not counter value.



ffclock_difftime(ffcounter ffdelta, struct bintime *bt, struct bintime *error_bound)
  - flagged as being the standard diff clock Cd
  - just adds error_bound reporting around fflock_convert_diff
     - but fflock_convert_diff just based on ffclock_convert_delta, so STill have ffdelta>=0
	  limitation!   hence still not a true Cd.
		  
  - BUG: same error as above regarding ps/s conversion factor. Comment citing /1e12 is correct but supplied value is /1e6 by mistake
  - ** the generation checking seems really pointless here as there is no bcopy,
    could just use ffclock_estimate.errb_rate directly in last line to get the 
	latest possible info, plus this parameter unlikely to change anyway
	 - well, I guess logic is same as bcopy protection, if ffclock_estimate is
	   in danger of asynchronous update, then all accesses to it must be protected.
		assignment to a field in a structure obviously is not atomic.
	 - but, this will not ensure that the err_rate value comes from the same stamp as the period,
	   in fact if the gen protection activates it will guarantee that it will Not! since stamp updates are rare, so it is not going to happen within both ffclock_convert_delta ANd the err_rate code.  So really, this is only to avoid having some kind of inconsistent meaningless value mid-update, just in case this is possible  (Ask Peter !!)
		And, why not use the ffclock_mutex  for this, since now I know that it will be visible here?  I guess you dont want to cause delays by Blocking access, only to ensure that you have a consistent value.  You are only checking for a very rare event anyway, dont want to block a key update just for that.
		
		
		
// Why are these defined where they are? better to collect all syscall/sysctl stuff
// together as I have done partially here
ffclock_[get]{bin,nano,micro}[up]time(struct {bintime,timespec,timeval} *{bt,tsp,tvp})
  - these ALL have the FFCLOCK_LERP flag, since this is what the typical
    abs clock reading expects of a system clock - only advanced users will want to 
	use Ca itself (and they will need support to help decide what to do if get 
	event reordering..  
  - this is another reaons why using even the FF system clock not good, it will
    always be the mono version. Need to get the raw pkt timestamps and use
	the RAD API. 
  - [up] fns dont have LEAP, as expected
  
  
ffclock_{bin,nano,micro}difftime(ffcounter ffdelta, struct {bintime,timespec,timeval})
  - wrappers to the ffclock_difftime(ffdelta, {,&}bt, NULL);  call
	 returning the requested format
  - naturally no flags needed since is a difference, as is native (not LERP) by default.





/* Syscall related stuff */

// SYSCTL tree:   [ underscores put between all new levels of nodes and variables ]
// created via SYSCTL_NODE, _PROC (for fns), _INT (variables) throughout file
// Must define each first, then insert into tree in order.

_kern  							   ""   // defined above kern_tc.c somewhere...
	-> _kern_sysclock	 			 "System clock related configuration"  // declared in timeffc.h
		- available;   				"List of available system clocks" [RO]
		- active      					"Name of the active system clock which is currently serving time"
		-> _kern_sysclock_ffclock   "Feed-forward clock configuration" // declared in timeffc.h
		   - version					  "Feed-forward clock kernel version" [RO]  // KV
		   - ffcounter_bypass		  "Use reliable hardware timecounter as the feed-forward counter"
		  

// related variables in this file gathered here
static int ffclock_version = 3;     // hardwire KV=3 choice
static char *sysclocks[] = {"feedback", "feed-forward"};
#define	MAX_SYSCLOCK_NAME_LEN 16
#define	NUM_SYSCLOCKS (sizeof(sysclocks) / sizeof(*sysclocks)) // length of array?
static int sysctl_kern_ffclock_ffcounter_bypass = 0;    			 // default is no bypass?
// and included via timeffc.h
#define	SYSCLOCK_FBCK	0        // indexes into sysclocks[]
#define	SYSCLOCK_FFWD	1
extern int sysclock_active;  		// defined in kern_tc.c



sysctl_kern_sysclock_available:
  - creates an sbuf containing the names of all clocks, separated by " ".
  - is written generally but so far, will only have two in there, assuming
    both FF and FB are supported, where is this decided?  having both is currently
	 hardwired if FFclock defined.
  - it destroys what it creates, how does string get passed back exactly?
	 - must be inside sbuf_new_for_sysctl, somehow links the sbuf to sysctl transparently
	   must grab it after sbuf_finish but before delete somehow.
  - sbufs:  sys/sbuf.{h,c}
      - for safe string formatting in kernel space: sbuf structure instead of char arrays
			- why are arrays unsafe?  how fix?
      - designed by Poul-Henning Kamp, first entered FreeBSD4.4
      - created with an initial size, last arg says if can extend or not
	   - sbuf_new_for_sysctl:  sets up sbuf with a drain fn to use SYSCTL_OUT()
		- last arg was "req", defined where?   look in sbuf.h
		- sbuf_cat:		 appends a null-terminated string
		- sbuf_finish:	 null-terminates string and prevents further modification
		- must have sbuf_delete call for each sbuf_new
		
	 
		
// from sysctl.h
struct sysctl_req {
	struct thread	*td;		/* used for access checking */
	int		 lock;		/* wiring state */
	void		*oldptr;
	size_t		 oldlen;
	size_t		 oldidx;
	int		(*oldfunc)(struct sysctl_req *, const void *, size_t);
	void		*newptr;
	size_t		 newlen;
	size_t		 newidx;
	int		(*newfunc)(struct sysctl_req *, void *, size_t);
	size_t		 validlen;
	int		 flags;
};
#define SYSCTL_HANDLER_ARGS struct sysctl_oid *oidp, void *arg1, intptr_t arg2, struct sysctl_req *req


sysctl_kern_sysclock_active:
  - if asking which one active, returns the string sysclocks[sysclock_active]
  		- uses generic  sysctl_handle_string  to do so
  - if changing the clock, finds the string matching the one given and stores
    the corresponding array index in global  "sysclock_active" directly
	 BUG:  doesnt use SYSCLOCK_{FB,FF}CK at all, using increased generality but not
	 working with current system.  This function relies on the order within sysclocks
	 matching that of the  SYSCLOCK_{FB,FF}CK  values.  In fact larger values than 1 can
	 be returned.  The defns of sysclocks and these labels should be linked.
	 I guess the user doesnt know about SYSCLOCK_{FB,FF}CK and these are not visible,
	 so they are told standard strings instead.. still, annoying.
  - ** small difference compared to watson.org version
  - returns error = 0 on success
  - how does one call this?  see other sysctl calls as inspiration..
  - obvious the deamon doesnt, and cant, call this, root needs to call it during boot
    I guess, based on some kind of config parameter set by the sysadmin.
- HISTORY ETIQUETTE:  additions in 11.2
		- changes are better, neater+clearer processing but wierd use of goto and continue
	   - find out who was author when ready and ask, potentially, before reverting those bits?
		- naa, they change my code without asking!  I removed the goto.  Still, find out who b4 committing.

sysctl_kern_ffclock_ffcounter_bypass:
  - defined and restricted to this file, no other occurrences, actual use must be via sysctl
  - question for Peter, where would missing code be?  this could well be work in progress, with no bypass yet in effect. ..
  - is this the switch for "passthroughmode" even though not used here since Xen 
    not included in release?
	 - in fact passthroughmode is about using TSC directly if available, it is not primarily a Xen or VM thing. See RADclock library doc.
	    - I think I have been confusing passthrough  (non VM) with bypass (VM)
		 - alternative is that bypass is the FF version of the same idea, for in-kernel
		   use only, with no provision for calling from some unknown user process.
			- In that case, could/should the daemon''s passthrough be exploiting the FF clock''s bypass?
			
	    - ** look at has_vm_vcounter:  looks like  bypass is for KV>1, passthrough is KV=1



	
			
										3 SYSCALLS:
						
// In case sysproto.h not yet included, but dont see why, is included at top
// I guess in some cases its autogeneration occurs, or finishes, later, this is a bug fix
// There is a convention whereby syscall "sys_name" becomes a function "name" in userland.
// Hence:  only see sys_names in kernel, not the names, and vice versa in userland

#ifndef _SYS_SYSPROTO_H_
struct ffclock_getcounter_args {
	ffcounter *ffcount;
};
struct ffclock_setestimate_args {
	struct ffclock_estimate *cest;
};
struct ffclock_getestimate_args {
	struct ffclock_estimate *cest;
};
#endif
 
int sys_ffclock_getcounter(struct thread *td, struct ffclock_getcounter_args *uap)
  - dont know how it can be that ffclock_read_counter wont run or will return zero.
    Lawrence didnt know either, nothing to do with threading.
	 	- protection is inside ffclock_read_counter here, but is gen based, not mutex
  - ffcount would be init to 0 anyway, right?
  - copyout (declared in sys/systm.h) is a specific routine for copying from k(ernel)
    (protected memory) to u(ser).  Usually its use is hidden.  Run by what thread? kernel?
  - thread argument is not used, and is absent in the linked userspace fns.  I guess this is
    for kernel use, to give it to the userspace thread that is calling it somehow.


int sys_ffclock_setestimate(struct thread *td, struct ffclock_setestimate_args *uap)
  - what is PRIV_CLOCK_SETTIME ?  where defined?
  - copyin:  again what thread is calling this? can it only be a kernel thread?
  		- use of mutex suggests read in by a kernel thread, but a different one to kern_tc
		- ahh, if ffclock_estimate referenced without mutex, because is already in kern_tc thread !  so can track threads in that way.  No! need protection in all places
  - note separation to getting stuff from userland first into a local var, before modifying
	the main kernel variable, namely ffclock_estimate defined in kern_tc.c
  - ** is the responsibility of the calling program to put data into the ffclock_estimate 
    format expected by the kernel (generic ffclock), how this is done will differ 
	depending on the specific ffclock
  - this fn increments (updates) ffclock_updated, signalling that new data is available,
    but this is not acted on immediately, only on next HZ tick
  - why use memcpy here if prepared to use bcopy elsewhere?  The bcopy is probably just copying the style of kern_tc which uses it, but not memcpy
  - protects writing to ffclock_estimate  global with ffclock_mtx
    Makes sense since syscall called by some other kernel thread (I think, otherwise mutex couldnt be used) on behalf of other (user!) processes, and needs to protect the thread running ffclock estimate reading code, to avoid the read getting mixed data.
	 For RADclock, should have only 1 daemon ever using this, so shouldnt ever need the protection, but better safe than sorry, what about 2 daemons??  must check that at most 1 can run sysclock at once.
  - Ok:  I think I see:  ffclock_mutex is a global, defined in kern_tc and declared extern in kern_ffclock, furthermore there is no global in the daemon code of the same name.
      So the variable used in {get,set} Must be a kernel-owned variable (in the timecounter
		`thread`, if that has any meaning) which is unique, annd in common to all user processes using the syscall.  Hence it will protect against all combinations of multiple daemons and librprocesses - easy .
	 
  - RADclock libprocesses cant use this syscall as the user version is called only via the wrapper kclock-fbsd.c:set_kernel_ffclock, and this function is only included in kclock.h, which is not included in radclock.h, so not exposed to library users.
     BUT, what is to stop an evil library from including it and wreaking havoc?


int sys_ffclock_getestimate(struct thread *td, struct ffclock_setestimate_args *uap)
  - this is the more sophisticated version of the old stored old values idea
  - Actually used in algo? good to have regardless for symmetry and future proofing.
     - yes, already documented
  - need protection against collision with a  ffclock_setestimate  call from the daemon.
  - the ffclock_mutex mainly used just for this.
			
If undef FFCLOCK, these are instead defined to just return(ENOSYS), so userland 
programs can learn that the FF not supported in the kernel.



// Thread issues:
Does multiple user processes trying to use same syscall imply multiple kernel threads each processing one of these syscall instantiations? end up with multiple kernel threads all trying to read, all in danger of clashing with write, hence all must be protected?
 - I believe yes, and clashed are prevented by ffclock_mutex  which is shared by all of them, either within the `tc kernel process` if that exists, else just within the kernel non-process.

Is there a `tc` (timecounter) thread, how to find this out?  where is it launched?
what is the execution here?  where is the blocking ?




/**************************************************************************************************/
/********* kern_tc.c ***********/

- opt_{ntp,ffclock}.h  usr/obj    generated automatically based on OPTIONS set
  (look in generic)   file should always exist even if empty? 
										
- FFclock is a compile time configuration switch meaning "FFclock support switched on", 
  and not "this is FFCLOCK code", still less "this is stuff we changed", 
  [not adding a piece without changing anything existing like before..]
  One of the new features we add is new functions valid even if FB is used.
  For the clock switching functionality to work at runtime, must define FFCLOCK.
 						
						
Reasons for tick based processing
  - need it to use 32 bit counters as they overflow !
  - standard approach for processing in kernel, otherwise would need interrupt based,
    and each different asynchronous aspect of the code would have to inter-operate
	 correctly and efficiently in that env, potentially with delay inducing locking.
  - Tick-based allows all things that occured intra-tick to be managed neatly on a
	 grid, including potentially a change of tc itself.  Grid is fine enough so that
	 the tick period delay (1/hz [s])  does not affect accuracy. Clocks are read using
	 interpolation over such periods anyway, generally..
  - provides simple means to have get/FAST variants


Reasons for a buffer of >2 timehands
  - could it be because some app is using an old one, and has to stick with it even after the timehands has moved on, so it needs to stay alive ?    And it needs to stick with it because ticks move on quickly, need to finish calculation, not repeat forever until you can fit it all in before tick-data changes (which is Under 1/hz), which may be never under heavy load, resulting in a busy loop!   Will be slightly late, but
      for most apps wont matter at all, especially if just processing a fixed raw already taken.
	  If system busy, could lead to backlog of windup calls, all the more reason just to stick with the tick you have and finish the job, before those calls change ticks in quick succession.
     Yes, this is it, see notes.
	  Note:  gen is not an id incremented over the array gen=generation, it means how many times you have wrapped the array.  Non uniqueness doesnt matter, only need to tell if have wrapped back to the current one since you started using it.  It is a wrap-count , which itself wraps
	  every 50 days or so assuming 1/hz = 1ms .
  - could provide some limited indexing into past clocks, to interpret past counter reads consistently, but see no evidence of this kind of usage, no cycle through the past to find the best `fit`, even though in some cases, ffcounts are processed that could be in the past.
      The whole buffer if only 10/hz long, typically 10ms..  not worth it.



						
						
						
						
/* Feed-forward clock estimates kept updated by the synchronization daemon. */
struct ffclock_estimate ffclock_estimate;
struct bintime ffclock_boottime;	/* Feed-forward boot time estimate. */
uint32_t ffclock_status;		/* Feed-forward clock status. */
int8_t ffclock_updated;			/* New estimates are available. */
 - values recognised are 0, >0, and INT8_MAX marking a ffclock_reset_clock call
 - normally gets set to 1 to indicate an update available, then set back to 0 (none available)
struct mtx ffclock_mtx;			/* Mutex on ffclock_estimate. */

struct timehands {
	/* These fields must be initialized by the driver. */
	struct timecounter	*th_counter;   // ptr to timecounter used at this time
	int64_t			th_adjustment;     // something to accumulate jumps?
	uint64_t		th_scale;          // period estimate as a 64bit binary fraction
	u_int	 		th_offset_count;   // cumulative uptime counter (but can overflow)
	struct bintime		th_offset;     // cumulative binuptime (no overflow)
	struct timeval		th_microtime;  // timeval  version of th_offset relative to UTC
	struct timespec	th_nanotime;   // timespec version of th_offset     "       "
	/* Fields not to be copied in tc_windup start with th_generation. */
	volatile u_int		th_generation; // reserved value zero used to mark "dont use"
	struct timehands	*th_next;
};

struct fftimehands {
	struct ffclock_estimate	cest;      // copy of kernel copy of daemon params valid at this gen
	struct bintime		tick_time;       // Ca at last tick (name is not bad), or with lerp?
	struct bintime		tick_time_lerp;  // mono version of Ca at last tick
	struct bintime		tick_error;     // why have this?  why not use cest->errb_abs ??
	                aah, error accumulates with each tick.. need to build in abs error
	ffcounter		tick_ffcount;		// seems to be full counter read at last tick, this IS the virtual FF counter, except when u read it, have to add in each delta from last tick
	uint64_t		period_lerp;         // adjusted period estimate according to mono-algo
	volatile uint8_t	gen;			  // gen=0 used to mark a timehands being initialised and unusable
	struct fftimehands	*next;
};

- BUG:  tick_error needs to build in cest->errb_abs  as well.
- tick_time_lerp seems to be the mono interpolated version of Ca, precalculated for this tick.
  -- where is the algo for the smoothing? it needs to be applied whenever clock read..
  -- seems to be in ffclock_windup, in fact that is a major role of the fn
  -- dont see why need to have value at last tick available, if always have to 
     add a delta to get to the current counter value anyway..
	  	- probably because easier to have all things tick based if anything is, for uniformity.
		- perhaps also to support the FAST reading option previously available
		- and of course, to support the FFcounter, will overflow unless go tick by tick
	 
- ifdef FFCLOCK then [get]{bin,nano,micro}[up]time() fns renamed with fbclock_ prefix,
  and the original names instead are wrapper fns which call the _fromclock wrappers 
  which understand both the FB and FF clocks.
  Note ffclock_.. based on workhorse ffclock_abstime, FB routines based on
  fbclock_binuptime  (originally called binuptime) 

- overlap of fftimehands with timehands
  -- th_offset corresponds to tick_time_lerp  (not tick_time, since FB is natively mono)
     th_scale          -> period_lerp
	 th_offset_count   -> tick_ffcount  ( but now always wide enough )
	 th_generation     -> gen
	 th_next		   -> next
   So if FFCLOCK, will have two forms of all of these?	
  -- timehands has
	 - access to the actual counter  
     - th_{micro,nano}time values expected by legacy fns
	 - ntpd magic: th_adjustment
  -- timehands has
	 - access to all underlying FF state params and error bounds  
     - a bound on error of the time at this tick (tick_time)
	
static struct fftimehands ffth[10];
static struct fftimehands *volatile fftimehands = ffth;

ffclock_init [static]
 - **bad name, should be "fftimehands_init", is ffclock_reset_clock that inits a ffclock "estimate"
 - here the circular buffer of fftimehands is being initialized more automatically
   than timehands was.
   - however the gen values are not set to be unique, all are set to zero, aah zero at the beginning, not just during an update
 - also sets other FFclock globals:   ffclock_{update,status}, so name better for that.
 - called by inittimecounter in this file only

ffclock_reset_clock
 - used only by inittodr in subr_rtc.c   [ rtc = real-time-clock , todr = TimeOfDay Register ]
 - takes a timespec from the RTC and sets FFclock to it, best effort for other fields
   The "boottime" for ffclock bintime_mul *bt) [static]
 - updates cest fields as well as FFclock globals like ffclock_boottime
 - simple conversion:   period * Delta-counter = Delta [sec]
 - notice the approach: assemble the local cest, then copy under protection when ready
 	- protection includes  setting ffclock_updated = INT8_MAX;
 - TODO:  bad name: typically only done once (well, maybe a cluster of times shortly after reboot)
   so `reset` is inappropriate, and not even
   symmetry with normal version tc_setclock .  How about ffclock_setto_rtc()  ?
 

 

ffclock_windup(unsigned int delta) [static]
 - used in tc_windup only
 - note ffth is a local pointer to a given tick in the array, not to be confused with the global ffth which is the entire array itself
 - here the input delta is the delta counter since the last tick, not estimate update!
   - ** timestamping location remains in tc_windup, this is just processing!
   
 - gen control:  note there is none, because This is the function that advances fftimehands,
   other fns have to protect against this advancement, but not this one :-)
	Thus here ffth is used not to keep the hand on a constant (current and soon-to-be-past)tick,
	but to point to the next one that will be overwritten, then become current.
	Thus "fftimehands" can be considered a constant, eg in bcopy(src,dst), and is used directly,
	unlike the tho=timehands  from tc_windup which is un unneccesary (but convenient) copy.

 - new gen setting:  in the end just set  ffth->gen to ogin+1  indicating that This tick has
 passed to a new generation (ie the array has wrapped around to it one more time. )

 
 - typical tick update
     - performed without first checking if a daemon update is available or if tc changed
       Cost small given only occurs 1 in poll_period*1000 times. Makes code
       simpler I guess.  I remember discussing this, probably suggested it.
		 Interpretation:  waste a little time so you can get lucky and catch the fresh update. If you dont, in fact no time wasted, if you do, the cost of wasted time is worth getting the fresh update into this tick.  If you reverse the order the total impact (over this tick and the next) is a slight work saving on the 2nd tick, at the cost of getting the update one tick later - not a big deal either way.  Principle is: earlier is good as want the daemon and FF clock to be as sync as possible.
		 BUG:  Hang on, much of that is based on the nuance that we are talking about un update occuring DUring this call, but the common case will of course be an update during the previous tick (if at all)
		     In that the waste of time will always be incurred. But, the code assumes this is ALways how it occurs, otherwise ffdelta = ffth->tick_ffcount - cest->update_ffcount;
			  wouldnt be +ve.  How to prevent this?  can the FFdata be updated during an interruption of tc_windup ??  I think so.
			      Ask Peter to double check.
		   - basically, can fail if update occurs in very narrow gap after tc_delta
			  called and if (ffclock_updated > 0)  tested
			  Super unlikely, but Fix by checking for either sign.
			  Adapt code used in ffclock_convert_abs  .



	 - as expected basic tick update just moves the two tick times and the counter forward
	   in the obvious way.  Order somewhat illogical however
	   -- BUG: repeats ffclock_convert_delta(ffdelta, cest->period, &bt);
		        in fact already stored in bt, no point in repeating calc
	   -- BUG  errb_rate constant is wrong, is mus/s not ps/s. Same error as in ffclock_abstime

  - other stuff initially just copied over

 - if (ffclock_updated == 0)  // not updated
   - here ffdelta represents how long ago from "now" the data was updated
   - fair enough, should get update every poll_period, 2*SKM_SCALE normally way beyond this,
     but what are the impacts of this state? what does it imply?  revisit after understanding
     user code.
   - ** if given up on future updates, should set period_lerp=cest->period, since dont
	   want monotonic fix continuing indefinitely - unless, lerp code would ensure this
	   anyway: more generally, does lerp code ensure that if Ca is already mono, that
	   that tick_time = tick_time_lerp after a certain time?  If so, then no need to 
	   contemplate making the daemon provide a mono version itself. 
	   In fact it seems to.


 - protection of cest reads
     - not needed when just copying static copy in current tick
	  - ffclock_mtx not needed when grabbing new one, because would not even be trying unless
	    ffclock_updated>0, and this is only true if sys_ffclock_setestimate has already finished the critical section.  Basically ffclock_updated is another form of protection.
		  Only possible problem is if ffclock_updated>0 but wasnt processed yet, then
		  when at last you start to process it, another update was in progress.
		  for this you would need  poll_period to be less than tc_tick or sth really wierd

 - if ffclock_updated>0  // updated
   - here ffdelta represents how long ago from "now" (tick start) the FFdata was updated, but tick_ffcount is not itself updated, it represents the start of the tick, and matches  timehands view of that same tick , as it should
   - BUG: (same as above) update will always be in the past? sure ffdelta cant be negative?
     -- tick_ffcount is from the raw delta passed in, if update returns in this fn, then
	    cest-update will be larger than it!
	  - tick_time recalculated - earlier update overwritten, but tick_ffcount is kept
       the same, as it is still the nominal raw time of the tick.
   - BUG:   repeats ffclock_convert_delta(ffdelta, cest->period, &bt);
   - ** ps/s error as before
   - hmm, uses tick_time_lerp calculated initially, ensure continuity of course.
	 Gets me thinking .. yes it
	 simplifies to make everything happen on a tick, otherwise the normal clock 
	 reading (which interpolates from the last tick to the current time), would have
	 to somehow be aware of an update and do all the reprocessing of lerp then..
	 messy, I recall this vaguely..

	- if (ffclock_updated == INT8_MAX)
	   - lerp is jumped!  what happened to continuity!  well after a reset what can you do..
		- so big comment block on status lower down is wierd, there is no gap if you have just reset, you just eliminated it !    Could it be that it is because ffclock_reset is only used in some architectures
		- but, reset also sets:  cest.status = FFCLOCK_STA_UNSYNC; so the big block covers a complementary case, why then does it mention the RTC?
		- aah, it resets cest.status, and that gets copied to FF on next update, but later the daemon recovers, but you are left with a heritage lerp that is now very different from it.  This then triggers (how? where) the FF to believe it is UNSYNC (overwritting the status from the daemon).  You want to kill the gap, but how?  modifying ffclock_boottime doesnt change the UTC clocks.


   - note gap_lerp is wrt to mono time, not what Ca was just before the jump..
     not what we usually think of as the jump, but is relevant to what to do with lerp,
	 this is precisely the new disparity we must drive to zero, and indeed we cant
	 jump lerp to take into account the update in any case by defn!
	  BUG:  well, stupid anyway: if ffclock_updated == INT8_MAX  then will know the value of gap etc, but calculated each time anyway..
   - forward_jump = 1 if jump up from lerp to native Ca from update
   - why bintime_clear(&gap_lerp);  if just about to assign in all cases anyway?   (ask Peter)
	  no doubt just a convention to initialize all variables.  Actually you just assign a
	  structure? Yes.  If the structure has no pointers, then no complex problems will
	  arise (like dangling ptr pb)
	 
   - Status updates:
		- be careful about ffclock_status (kernel status) and "cest->status" from daemon 
	   - ffclock_updated == 0  [ RTC stuff again]
		   - uses FFCLOCK_SKM_SCALE (only place used)
	   	- should really (or at least also) use info on pollperiod (could be obtained through new valid_till field from daemon), otherwise kernel code can be set to UNSYNC even if everything is perfect for that pollperiod !  consider that SKM is around 1000sec, but poll can be 1024 !!   aah, what is maxpoll?  mabye 2*SKM is safe, but principle is wrong, and need for separate copy of SKM in kernel.
				See valid_till DISCUSSION
	   - ffclock_updated > 0  [ later on ]
	      - Condition reads: if  UNSYNC in kernel but not in daemon, and in WARMUP, then
			- absorption of jump into boottime similar to what I did by changing the origin K
	        to maintain continuity at t following a pbar update.
		   - BUG: contrary to comment block, absorption only works to make the CaMono have a cts uptime clock, I think,
			  Ca will jump by a different amount at the tick-start boundary in general
	      - kernel status set to daemon status, this fails in rare case where kernel knows best,
		     see ffclock_change_tc
					
   - lerp recalc:
  	   - moronic comments fail to describe the objective..
		 From memory, my idea was to:  reduce current gap to zero at next tick, subject to cap 
		 Advantage is keeps modif in the zone where the algo doesnt touch, inbetween stamps.
		 Actually, idea was to reduce to zero at next STAMP, next tick would be very demanding.. on the other hand, how does the FF know the polling period?
		 See valid_till DISCUSSION.

	   - period_lerp initialized to current cest->period, discard old value, 
	      -- is neat, start afresh, no complex dynamics
		   -- and if gap is zero, it stays that way
	   - this time ffdelta is the counter-time between daemon data updates
	      -- again assuming it is non-negative, but this time is good
	   - thus "polling" estimates polling period, but rounding would be better..
	   - bintime_mul(&bt, polling) = expected worst case gap at next data update, then 
	     cap gap_lerp at this value  
	   - frac calculation
		   - frac -= 1,  but frac is unsigned... does this make it all 11111 ?
			- TODO: is this per-second processing to make it per second somehow??  probably some
			  kind of overflow or resolution management issue?  check
			- frac is just pieceofgap/countercyle, hence in period units
			- sign is correct:  to catch up +ve gap, need CaMono to run faster, hence want
			  each cycle to count for more [sec], hence larger period_lerp

	
	- indicate update processed

 - point to fftimehands to new updated structure



-------------------------------------------------
DISCUSSION:  use of cest->valid_till   in kernel  and input into FFCLOCK_STA_UNSYNC
  - in fact not legitimate to pass a parameter indicating when FFdata becomes unreliable/old
		- if a soft decision, already have parameters for this:  errb_{abs,rate}
		- if a hard or clever decision, already have (many!) status bits could use
		- anyway, what would the kernel do? the daemons job to sync, is no FF fallback algo
		- if can report that state and error is bad, that is enough, still doing the best it can
	   - anyway, cant be a measure of error, or time elapsed, without also reporting poll_period, because daemon error may be perfectly normal given the CHOSen poll_period
	- Guiding principle:  the daemon is the expert, only mistrust in extreme cases!
	  So only real topic here is When to mistrust :  full list is:
		  i)  when daemon tells you via status   (not by error!)
		  ii) when daemon not updating:  (either dead, or GAP in valid stamps (eg server down))
		  ii) if tc change confusion in-train, but should be handled separately via:
		      - deamon detects change, sets state as bad, does full (or almost full) restart !
	- for (ii) what IS legitimate is information on when next update is expected.
	  FF can then measure itself how long since the expected next update, as well as
	  how long since the last update
	    - how old:  a measure of how bad things could be
		 - how late: direct evidence of daemon death
		 - how old is too old:  via for eg SKM_SCALE, and FFCLOCK_SKM_SCALE already exists for
	      this.  As a rough universal value, that is ok.
	  With (how old,how late, SKM), can now make reasonably robust determination of when assume daemon non-reponsive, then can set FFCLOCK_STA_UNSYNC, override current cest-status (reset, or just ignore?) as deemed unreliable, and take other actions?
	      Logic should be:  if way over SKM and update very late, then USYNC

	  When new update comes in, all good, even if new status bad, standard processing should apply  [ so not so tricky in fact, tc-change aside, check logic ]
	- Estimating expected next update:
		- basic estimate is poll_period, in which case no need per-update, is just a constant,
		  in which case, how to communicate to daemon?  Sysctl tree is for controlling FF,
		  it should not contain daemon info, only cest is allowed to have that.
	   - but, poll_period can change (really? still?) in which case, a per-update makes sense,
		  it will not just give the value but also Signal the change within an existig mechanism
		- but could have gaps, maybe daemon could give a smarter worst-case update time including
		  measured gap sizes, but how can it know when a gap coming?
	   - Simplest estimate is just gap between last two updates seen, and FF can measure this,
		  in fact already done inside ffclock_windup ,  but could be huge if there was a gap,
		  want a more robust estimator, but dont want complexity in kernel
	   - gut feeling is, just making valid_till=poll_period  the Simplest
		  - format:  FFcounter?   simplest for comparisons, and calculating maybe, suits daemon
		  			 	 bintime?     more immune to tc change issues, but restarting then anyway..
		    either way, will need some conversion code
	- valid_till currently only used on daemon side within  raddata_quality(), which is
	 still very experimental, do we really want to put such a feature in the kernel?
	 On the other hand, this is not about its daemon-utility, but its FF-utility.
	 There is bound to be another reasons down the track why FF needs to know it.
	 The memory is trivial, and it reduces computation in kernel if result just given.
   - if keep it and its meaning is poll_period, should change its name to timetoNextUpdate  or update_expect or {time_}nextupdate), and clearly cant be set in FF-->algo dirn.
		 next_expected  matches the flavour of last_changed, where `time` is implicit.
	    Tempting to call it poll_period also, but since the units have changed and the intent is
		 different, a different name works.
		** Ahh, confusion, I said make it poll_period, but really want it to carry poll_period in the form of
		   a raw timestamp,   ie  next_expected = last_changed + poll_period/phat  .
			This matches the semantics in the daemon code where it is a raw timestamp, not an interval !

	- same logic applies for daemon use as well:  (how old,how late, SKM) gives you the full
		set of info you need, and using FFcounter format is convenient there, as already implemented.

	- summary:  [fixed]
		 valid_till:   keep it, rename to next_expected.
		 					will be Tf + poll_period(counter units),
		status logic:  if way over SKM (time based)
		                  & update very late (number of missed updates based)  then USYNC
                     perfect... this is nice, and obvious !

-------------------------------------------------





/* Mono-clock/lerp discussion */
Qn: should the mono-clock based on Ca be generated in the userland code or the kernel?

Reasons for algo: 
 - principle of keeping all time intelligence in one place, in the algo
 - should in any event provide full gamut of specialized clocks: Ca, Cd, Ca-mono, scaled counter
   it is not only the kernel that needs a mono-clock option!!  Currently clock calls from 
   userland (dont even know how that works yet) wont have such access. 
 - enables innovation on the mono-clock perf without requiring kernel modifs. 
   currently the algo can propagate large absolute errors, eg a jump of a few ms will take
   a poll_period to dissipate !  A smarter, or more configurable mono-clock, could
   catch up to Ca much faster, for example with no smoothing at all in the case of 
   forward jumps.
   -- needs a study, total error could be helped by it also when Ca noisy
   -- could have different kinds of mono-clock obeying different smoothness criteria,
      and the one push to the kernel would still be a conservative one
   -- but: cant make it more complex without increasing user->kernel communication,
	  or making the per-tick more complex, and in userland, the mono-clock reading 
	  more complex also. 
 - if per-stamp based as it is currently, may as well do the lerp recalc code in the daemon anyway,
   could keep much of the per-tick update code unchanged
 - easy options for communicating to the kernel: extra lerp member in cest, plus
   some kind of flag saying if lerp is reliable (maybe in status), and a kernel
   option settable by sys_ctl to use the kernel lerp backup code instead, and this
   would be done anyway if the damon lerp was unreliable.
 - if Ca is already mono, this is a 2nd algo on top to no purpose..  
				
Reasons for kernel:
 - currently the algo is per-stamp, cant do anything intra-stamp (well, except read it..)
  so the kernel mono algo is complementary.
 - faster convergence requires a userland process operating intra-tick  (another 
   time-scale to manage), effectively pushing new data more frequently - more overhead.
   -- or, a more sophisticated per-tick update which slows down after a precalculated number
      of ticks, a target time typically much closer than the next data update
	  Would add another test on each tick - that is ok
 - mono is ESSential for the kernel (eg all the absolute time read fns are hardwired 
   to use lerp) so it needs a facility, what if a given FFclock daemon
   doesnt provide mono? or it is unreliable?
				   
Current thoughts:
 - retain the super simple intra-stamp complementary algo as a needed backup
 - augment userland to provide a mono-clock also, included in ffclock_estimate
   as well as thread server userland timestamping (standard plus RAD API).
    -- this can be pushed at finer timescales than poll-period if needed without
	   changing ffclock_updated communication.  The kernel pushing thread can hold
	   the intra-stamp mono code, will still be in userland, but RAD algo unaffected,
	   but finer level data available to make mono smarter
 - provide switches to choose one or the other, plus compile time default, fallover
   to kernel mono in case of problem.
 - short term: maybe leave this as a todo, note it needs plenty of userland side support, 
   and just including a mono field in ffclock_estimate will not cut it, need the
   other parameters to specify how intra-stamp interpolation works, so would have
   to include a lerp->period type field as well, at a minimum.  Plus without 
   pushing at sub stamp timescales, all this would just be to relocate the existing mono 
   algo intelligence to userland, and even then, the kernel still has to know how
   to use the mono parameters in per-tick updates, so 100% decoupling not possible.    


/*************************************/
  

ffclock_change_tc(struct timehands *th) [static]
 - used in tc_windup only to handle FF impacts of a counter change
 - is passed the new timehands, after the FFcounter member has been updated!
 - is called after ffclock_windup is called, yet advances to the next fftimehands,
   but ffclock_windup already did that !
 - basically just copies the just-updated fftimehands info in, ok for tick-start fields,
  but even keeps fields like period_lerp
   and tick_ffcount! that will be meaningless stale state for the new counter.
	Actually need to set tick_ffcount=0, or ncount
   I guess it is the daemons job to repopulate these when it can.
   It does do a reset-like resetting of the kernel copy of the ffclock data (the 
   estimate) and admits is it FFCLOCK_STA_UNSYNC
  Maybe better strategy is to refuse to move from the old tc until it has evidence
  that the daemon has begun updating from it.. but should still store the new counter value
  and start tracking it, the FB is .
 - "communicates" indirectly with the daemon by delaying an update, I guess because
   the daemon May see the next stamp (using the new counter on the backward dirn 
   only or both) as just bad quality (or even good since it cant even filter properly yet), 
   and not set the status appropriately. It would
   then push a repetitive set of parameters, including a good status which would
   be very bad.  Basically, here the kernel knows better than the daemon rather
   than the other way around.  This is important: is there a better way to force
   the daemon to switch to FFCLOCK_STA_UNSYNC and to reset itself? asynchronously 
   wrt stamp arrival?  This would be a special status that is both read and written by 
   the daemon and kernel

	Already have sysctl to see what the counter is:
	 sysctlbyname("kern.timecounter.hardware", &hw_counter[0], ..)  // see if tc there or changed
   Note asychronous, but in daemon after the algo runs on a new stamp, is used to
    record tc in struct radlock member hw_counter[32];  initialized in radclock_init_vcounter
	 Why not test for this as soon as a new stamp is formed? or raw data comes in?
	 should we flush the raw data queues?   Seriously a full restart would be best..
   Acted on in
	  pthread_dataproc.c:process_stamp  to reinitialize, but no thread restarting, just
	      some parameter resetting, like peer->stamp_i = 0, stats left alone..  what does this do
			It does promote an immediate return(0), thereby skipping the pushing to the
			kernel and the rest of the function - effectively skipping this stamp (in which case better to do this before the call to process_bidir_stamp?)
			however, (0) is the code for success, and will not trigger any restarting.
			Need to look at this in depth - how to restart the algo properly but not the daemon?
			??  Need algo documentation for this first, but resetting everything manually
			could be complex, is an ideal opportunity for a full restart which would also flush
			the raw data buffers, full of halfstamps from the old counter..
			Some stuff you want to restart, like trigger, but maybe easier.
		  Acutally you Want to push an update perhaps, to say immediately that the daemon is
		  unsync !! and to set parameters to restart-type values !


	  virtual_machine.c:  ?  forget for now

										
ffclock_last_tick(ffcounter *ffcount, struct bintime *bt, uint32_t flags)
  - just retrieves the last relevant absolute time and counter value stored at last tick
  - used in ffclock_abstime in kern_ffclock.c only
  - here flags are passed in, but other args are used to pass back
  - 2nd part of comment unclear, doesnt gen take care of any issues?  anyway 
    ffdelta not even in this fn, so comment is out of place, probably just copied
	from fflock_convert_abs
  - the check on gen is to ensure that a new result has not just become available
    since gen first set, if so, this data would be out of date, go back to get 
	the latest info. Not really a qn of consistency, since gen not past back anyway.
	

ffclock_convert_abs(ffcounter ffcount, struct bintime *bt, uint32_t flags)
  - low level Ca routine
  
  - only used in ffclock_abstime in kern_ffclock.c
     - similar to ffclock_convert_diff, they are defined in kern_tc, but not used there
	  - they do make direct use of the timecounter global  fftimehands , maybe that is the
	  definition of kern_tc membership.
	  			YES!
	  
  - almost a subroutine but uses external fftimehands
  - allows ffcount to be either in future or past of current stored tick time
      - manages sign difference manually as ffdelta used by ffclock_convert_delta must be +ve
  - uses the ffclock stored at the last tick as a basis, then interpolates using
    the relevant period to the given counter value, which can be in the past.
  - does this completely separately for two kinds of absolute clock:
	-- native Ca (using tick_time      and cest.period )
	-- mono-Ca   (using tick_time_lerp and period.lerp )
	mono details must all be in ffclock_windup
  - this increment method makes sense inbetween stamps, where K - thetahat is 
    a constant anyway, so can interpolate between stamps with some rate, and hence
	between ticks falling in that interval in the same way. Avoids the need for the 
	kernel to be aware of K and thetahat, or even their difference, store it all
	in the final bintime.
  - if in past, not clear if current tick is best, should be the nearest tick. 
    Clearly this is meant to just cover the near real-time case where the last tick
	probably is the closest. As a refinement, could go backward in the timehands
	chain to find the closest out of them.. vague memory of discussing this issue.

Gen checking:  not all access protection is the same.. this is how it works here I think, closely
linked to the underlying purpose of the timehands circular buffer.
Simplified code:
	do {
		ffth = fftimehands;  	// grab copy of current value of global ptr, it may move
									   // on to next buffer posn at any time following windup call!
		gen = ffth->gen;        // grab a unique-enough id for the `tick` ffth is pointing to
	   dostuff based on ffth   // all work based on the (constant) info in this tick
	} while (gen == 0 || gen != ffth->gen);  // repeat until id is valid and hasnt changed,
	                                         // confirming work was wrt a valid, constant tick
		  
	In more detail:
		 Idea is that gen can only have changed if the timehands advanced all the way around the
		 buffer, or more, so that the tick contents have been updated.
		 Theoretically gen=0 impossible when first assigned in which case the dostuff wont been
		 a waste of time, and gen==0 whoud be superfluous, as gen != ffth->gen would catch that case.
		 Better safe than sorry I guess, just in case it was gen=0 initially, in which case the tick
		 data was in flux. OR:  all the gens are initialized to 0 in ffclock_init, so this waits until
		 windup runs for the first time, which will then set gen=1 .
	
	
	
	
static void ffclock_convert_delta(ffcounter ffdelta, uint64_t period, struct bintime *bt)
 - ffdelta and period passed, result ffdelta*period  returned in bt
 - best interpretation: not basic Cd but just a simple multiplication fn  using binary tricks
 - ffdelta must be +ve because ffcounter is unsigned !!
 		- so -ve time intervals must be handled in smarter routines calling this
 - ffdelta passed by value so it can be used to count down remaining cycles
		
 
void ffclock_convert_diff(ffcounter ffdelta, struct bintime *bt)
  - low level Cd routine, taking supplied ffdelta, but grabbing current
  period itself from the FF data  (ffclock_estimate)  safely using gen checking
  		- hardwired to ffth->cest.period, not ffth->period_lerp, since
	   	difference clock has nothing to do with lerp version of Ca, of course.
  - then just calls ffclock_convert_delta hence same conditions apply
  		: delta must be >=0
  - better name would be ffclock_convert_delta_safe, as this described what it does better,
  and since not flagged as the Cd as such




ffclock_read_counter(&ffcounter):
  - 'simply' reads the counter and returns it
  	- does it via `last + delta`, instead of just reading it, because this IS the delta code synthesizing the FFcounter !!!
  - note we are trusting tc_delta to return a meaningful delta that has dealt with wrapping
    somehow, since tc_delta is based on  th->th_offset_count   So, the FF counter, is based
	 on the FB counter wrapping code !
  - used only in kern_ffclock and kern_tc - not in bpf !  (bpf doesnt have timehands access?)
  - exploits traditional inline tc_delta() to get counter delta to add to tick_ffcount
  - timehands and fftimehands are externals defined earlier in file
  - gen test is needed because dont want delta to be the same generation as gen,
    and tick_ffcount to be the next one (with that delta already built in plus a bit more!)
	 - but this [ ie using the timehands'' gen, not the fftimehand''s gen ]  assumes that timehands and ffttimehands are perfectly in sync, yes? true?
	   why even separate them?
      BUG?  It is true they are highly syncd, but not certain this works, could have an interrupt at wrong place inside tc_windup.  Check it.
   - Ok this is it (cf last Peter email): grab the th gen and the ffth gen.
	  These are volative hence
	  expensive to read so get them once then stick them into locals so you dont have to read
	  then again (because compiler Would read them again each time otherwise).
     Key concept is that from then on are always looking at the very same buffer members,
	  and in fact that is what you want!  hence the copies.
	  The current ticks may have moved on, but you wont.  The gen check is to see if these (fixed) members how have different data inside them (or uninitialized), indicating a complete wrap of advancement of timehands and fftimehands in the interim !
	  The test can be, in fact Must be, based on th not ffth, because fftimehands can be advanced
	  in ffclock_windup before the end of windup, where th is a test that the entire FB+FF update is complete - hence that comment is misleading
	  Compiler view:  if in fact (ff)timehands werent expected to change but may,
	    the creation of the local is just a means to tell the Compiler
	    to treat the variable normally, rather than according to the volative declaration.
		 The compiler will therefore use it normal processes to see if a variable has changed
		 or not and only refetch it it things is has, otherwise use the value in RAM.



  - seems very complicated but cant avoid this if native counter less than 64bits
	If native is 64, timecounter currently dumbs it down to 32 ..
	But, still could have a direct access via rdtsc in case of TSC, why not??
	then wouldnt need to use the tc approach of "last tick + delta" at all !!
	At the very least this fn would be simpler..  but what if counter changed mid-flight?
	would be complex to have a special case..
    -- would be very useful to compare against pre-BSD RAD kernel version with
       tc implementation to see if done simpler than that there
	 -- Ahh, maybe this has nothing to do with adding deltas because of 32 bit problem,
	    but a fundamental decision (modelled on copying/interworking with existing approach,
		 plus existence of the interrupt cycle) to not have a `K` type variable as in
		 Ca = TSC*pbar + K, but to always build on a previous recent value.
		 A pity, it is complicating things a lot!  Note though that would still need to be able to index into past to get right parameters, and such an indexing would be RADclock specific in a way, or a least Ca=TSC*pbar+K type specific. Here this is done but the HZ-tick memory of the timehands array. When params change (much more rarely than tick) then relevant changes recorded in the timehands, but no K update..  Ahh, this means that if RADclock jumped back, then this cant be reproduced, is timehands already inherently partly monotonic even without LERP?
		 What a mess.
  - defn of ffth unneccesary, just wastes time, doent add safety or clarity, but
	 probably obeys conventions

										


997 /*
998  * System clock currently providing time to the system. Modifiable via sysctl
999  * when the FFCLOCK option is defined.
1000 */
1001 int sysclock_active = SYSCLOCK_FBCK;

- this is the only time the variable is set! so if you want to use FFclock, must
  select at runtime (possible only if FFCLOCK defined)
  
										
============================== Non FF code ==========================
struct timehands {
	/* These fields must be initialized by the driver. */
	struct timecounter	*th_counter; // ptr to timecounter used at this time
	int64_t			th_adjustment;     // a kind of offset fix from adjtime(2)
	uint64_t		th_scale;          // period estimate as a 64bit binary fraction
	u_int	 		th_offset_count;   // cumulative uptime counter (but can overflow)
	struct bintime		th_offset;     // cumulative binuptime (no overflow)
	struct timeval		th_microtime;  // timeval  version of th_offset relative to UTC
	struct timespec	th_nanotime;   // timespec version of th_offset     "       "
	/* Fields not to be copied in tc_windup start with th_generation. */
	volatile u_int		th_generation; // reserved value zero used to mark "dont use"
	struct timehands	*th_next;      // next tick data
};
- th_{micro,nano}time are the precalculated UTC stamps accessed by the 
  get{micro,nano}time fns, but not the others! eg not even the "get...up" functions
- th_adjustment only used here in tc_windup, is set by "the NTP PLL/adjtime(2) processing"
- th_generation: each new timehands gets an incremented value, like our 
  {counter,clock params} database to determine what
  the clock params were for a given raw stamp, but only use the 10 most recent.


...
										
static struct timehands *volatile timehands = &th0;
struct timecounter *timecounter = &dummy_timecounter;
static struct timecounter *timecounters = &dummy_timecounter;
 - timehands used to point to a record of what was current at the last tick,
   including the timecounter used at that time. At the next tick this will have
   changed if the selected hardware counter was changed

- timecounters are linked together, in tc_init "timecounters" set to last one created
- "timecounter" points to the current timecounter used, can be changed to a better
  one by tc_init

110 struct bintime boottimebin;
111 struct timeval boottime;
 - bintime clocks are natively uptime clocks (time since boot). To get UTC
   add boottimebin to them
   
   			
										
								3 SYSCALLS:

SYSCTL tree:   [ underscores put between all new levels of nodes and variables ]

_kern
  - boottime  "System boottime"
  -> _kern_timecounter
	   - stepwarnings	"Log time steps"
	   - hardware       "Timecounter hardware selected"
	   - choice			"Timecounter hardware detected"
	   -> kern_timecounter_tc										
			-> kern_timecounter_tc_"tc_name"?  "timecounter description" 
				- frequency	"timecounter frequency"
				- quality	"goodness of time counter"
				- counter	"current timecounter value"
				- mask		"mask for implemented bits"
										
- note address of local variable is passed instead of real one - standard for 
  safety no doubt to avoid userland altering protected memory

sysctl_kern_boottime  [static]
- returns the components of boottime tval

sysctl_kern_timecounter_get  [static]
- returns the counter reading according to timecounter (not wide enough..)

sysctl_kern_timecounter_freq  [static]
- returns counter frequency stored in the timecounter
 
....  and after tc_windup


sysctl_kern_timecounter_hardware(SYSCTL_HANDLER_ARGS) [static]
- called by _kern_timecounter_hardware leaf  "Timecounter hardware selected"
- first it just reports back current timecounter, returning "error"  (good thing..)
  If there is a problem, it looks through all timecounters looking for a match 
  to the name "newname", and sets the current timecounter to it. 
  If it cant find it, returns EINVAL
- code is confusing, I think it is because hidden inside the call to 
  sysctl_handle_string is the user request, which can be either to get, or set, the counter.
  In the set case the name of the current counter which is passed is ignored and
  overwritten with the desired one.  I think Lawrence mentioned sth about this kind of thing.
  
sysctl_kern_timecounter_choice(SYSCTL_HANDLER_ARGS)	[static]
- called by _kern_timecounter_choice leaf  "Timecounter hardware detected"
- cycles through all available timecounters and prints name and quality for each
- ** comment is the same as the previous fn, this is a mistake.
										


179 /*
180  * Return the difference between the timehands' counter value now and what
181  * was when we copied it to the timehands' offset_count.
182  */
183 static __inline u_int
184 tc_delta(struct timehands *th)
185 {
186         struct timecounter *tc;
187 
188         tc = th->th_counter;
189         return ((tc->tc_get_timecount(tc) - th->th_offset_count) &
190             tc->tc_counter_mask);
191 }

- note everything based on what is recorded in the timehands th passed in:
  the tc used is recorded, and the th_offset_count was made using that tc.
  Hence we guarantee here that the T, and Told values in T-Told are made by the same tc.
  Need to be careful because the tc is allowed to change from one tick to the next, and
  in practice would actually change asynchronously in mid-tick no doubt.
  You never want to be caught comparing different counters !!

- really obvious question: how does this deal with wrapping !! does the mask do it in addition
  to getting rid of useless higher bits !  cant believe didnt think of this before..

// Two new fns decided to support comparisons between clocks (eg for researchers)
// This, together with underlying info structures  ffclock_info and sysclock_snap,
// seem to be the modern version of faircompare
//   - some parts protected by FFCLOCK, but whole fn is new code
//   - oops, not just comparison, sysclock_snap structures seem to be the way that
//      FFclocks are used in bpf !  are they stale? only up to last tick?
//     Yep, this is the Universal sysclock way to do bpf timestamping, is simultaneously
  - the new FF support (disabled if !FFCLOCK) , and the new FB support.  May be easier
	 to first understand the FB version, see what kind of bpf headers it uses.
  - nice that these are above bpf level, defined in kern_tc after all, yet the key to
   bpf functionality:  some modularity at last, and means I can understand this completely without
	a problem

sysclock_getsnapshot(struct sysclock_snap *clock_snap, int fast)
   - only used in bpf.c !
   - must fill cs structure arg
	- name should be sysclock_getstate  or _getstateandtimestamp  _getallstate
	- ** this is the expanded and Universal version of `read counter now, convert to time later`
		- regardless of what clock active, can read either one after the fact, and hence do
		  faircompare, and know what the system clock would have returned at the time
	   - this seems complete, but does a lot more than just return ffcounter, generalised to
		  {delta, ffcounter}.  Of course FB cant be read retrospectively like the RADclock API..
        and neither can the FFclock.  So this function is also providing an in-situ replay ability.
		  But the perf penalty in time and memory?

   - Idea is simple:  store all the {ff}timehands state needs to read the clock, together with
	  the counter info.
	  Pros:
	    - natural extension of `read now, convert later'.
		 	- reveals that FB and FF not so different, if keep state, can also convert FB later
			- since need delta even for FF, logical to do both FB and FF
			- since dont know flavor of final conversion, adding a convenience conversion in the same
			  fn indeed makes no sense, so ok to split off into  sysclock_snap2bintime
 		 - can convert later  [ though most work done, no longer lightweight, so no big deal per se ]
		 - can convert flexibly:  (LERP,leap,fast,FF/FB,active sysclock,just raw)  changable
		 - can read either clock regardless of  active sysclock at the time, and compare against it
		 - can use to generate a clock-param database at tick resolution for replay applications
		 - only need to capture state once, to support multiple reads, each with their own
		   (LERP,leap,fast,FF/FB,active sysclock,just raw)  preferences.
		   - useful if multiple processes tapping same packet, provided can pass this info to them!
			- means all using same raw data, hence perfect faircompare methodology
			- supports formally FB versus FF comparisons
			- extends it to mono version of FF, and leap treatment
	 	 - compression of the two leap members makes sense here because the counter is now fixed,
		   hence so is the leap adjustment required.  This way do the leap work once only, even though may never be used.., but avoids worst case of many taps doing it
	  Cons:
       - onerous at high capture rates:
		 		- a lot of state per pkt
				- usually dont have a lot of processes hanging on same pkt
				  (SHM with UDP an unnecessary exception)
				- in terms of database generation, generates per-tick instead of the per-update !
				  so huge amount more data, and less accurate than RADclock API, but this is
				  kernel, probably wouldnt use it for this purpose
	    - no fallback to lightweight basic need for FF:  just the ffcounter.
		   Can the flag be extended to support `basic`, but then the split with sysclock_snap2bintime
			to do the generation wouldnt work..



	- this seems to be part of `The` methodology to improve bpf timekeeping. If it can be arranged so
	 that it is only called once for any packet (with more than one filter listening on it), then
	 the completeness of the snapshot data allows for the receiving program to do whatever it wants:
	 read FF or FB, add in leap or not, LERP or not, follow the existing sysclock or use the clock of its choice. Since it is raw data, could even use it to replay usinf the Algo of its choice, by ignoring params and just using the ffcounter (in the FF case anyway)

	- It is called even if its FF components
	are disabled via FFCLOCK (hence it is not itself protected, it is a new FB/FF universal fn.)
	- seems heavy though, a lot of work per packet !   and no reuse of existing read
	  fns, everything done from scratch again, even though they are all available!

 - basic thing to grasp is that
     if !FFLOCK  then no FFclock stuff exists so CANT attempt to fill FF fields, so may as well comment out that code: it can never be executed
	  if FFCLOCK then everything exists, but must cater for either FF or FB being the active clock.
   Note the value ofsysclock_active, but has no impact on execution, both clocks are recorded if
	at all possible, regardless.  In any event, the presence of daemons is not needed here, and FB must always be defined in the kernel in the current arch otherwise no tc exists .



Always (regardless of FFCLOCK)
	 delta				// uses tc_delta, or just 0 if fast
	 sysclock_active  // records global  sysclock_active,  does not act on it

	 FB branch
		Fills all FB fields of input empty variable fbi  // fbi = &clock_snap->fb_info;
		fb_info:  // should be fb_state
		  th_scale	  // th_scale from copy of current timehands
		  tick_time	  // th_offset      "				"
		  status      //  uses external  time_status  BUG: doesnt use fbi to get this
		  error       //                 time_esterror  BUG:  ",  also, calculation seems wrong
		  			BUG:   bt.frac = ((time_esterror - bt.sec) * 1000000) * (uint64_t)18446744073709ULL;
				       			 need (time_esterror - bt.sec*1000000) * MUS_TO_BINFRAC  where
#define MUS_TO_BINFRAC = (uint64_t)18446744073709ULL; // 2^64/1e6
               TODO: should this error be calculated if fast?? or at all? seems time consuming
					TODO: should these constants be applied to cputick2usec also even though they are LL not ULL?
					 		also, should ask the author first?


FF branch   (if FFCLOCK)
  ffcount = ffth->tick_ffcount + delta  // read FF counter (but if fast, delta=0)
  Fills all FF fields of input ffi   //  ffi = &clock_snap->ff_info;
  ff_info:
    leapsec_adjustment 		// calculate using both leapsec fields in cest in usual way
	 tick_time{_lerp}, period_lerp	 // take values directly from ffth
	 period, status     // take values from ffth.cest
    error        // initially take from ffth->tick_error
	              // if !fast, inflate with  accumulated error using cest.err_rate
					  // This is similar to ffclock_windup, but correct !  check carefully later






=================== Reminder of structures ===============================================

// From kern_tc:

/* Internal NTP status and error estimates. */
extern int time_status;
  - defined in kern_ntptime.c  takes values in
STA_{MODE,UNSYNC,CLOCKERR,PPS{TIME,SIGNAL,FREQ{HOLD},JITTER,WANDER,ERROR},PLL,RONLY,NANO,CLK,INS,DEL,



extern long time_esterror; 				// holds number of mus of error?

time_t time_second = 1;
time_t time_uptime = 1;


struct timehands {
	struct timecounter	*th_counter; // tc used during this tick [ counter access + other tc data ]
	int64_t		th_adjustment;        // adjtime(2) adjustment: ns/s as a 32 binary fraction
	uint64_t		th_scale;          	 // period estimate as a 64bit binary fraction
	u_int	 		th_offset_count;   	 // counter value at tick-start (wraps)
	struct bintime		th_offset;      // uptime timestamp at tick-start (no overflow)
	struct timeval		th_microtime;   // UTC timeval  version of th_offset
	struct timespec	th_nanotime;    // UTC timespec version of th_offset
   volatile u_int		th_generation;  // wrap-count to ensure tick state consistency
	struct timehands	*th_next;       // pointer to next timehands variable in cyclic buffer
};

The basic idea is that uptime timestamps are calculated based off the tick-start as :
	C(t) = th_offset + th_scale*( tc(t) - th_offset_count )


struct fftimehands {
	struct ffclock_estimate	cest;      // copy of kernel FF data from daemon used during this tick
	struct bintime		tick_time;       // UTC Ca timestamp at tick-start
	struct bintime		tick_time_lerp;  // monotonic version of UTC Ca at tick-start
	struct bintime		tick_error;		  // bound in error for tick_time
	ffcounter		tick_ffcount;		  // FF counter value at tick-start (doesnt wrap!)
	uint64_t		   period_lerp;        // adjusted period used by monotonic version
	volatile uint8_t		gen;		     // wrap-count to ensure tick-data consistency
	struct fftimehands	*next;        // pointer to next fftimehands variable in cyclic buffer
};

The basic idea is that UTC timestamps are calculated based off the tick-start as :
	Ca(t) = tick_time + cest->period*( T(t) - tick_ffcount )




==========================================================================================


/*
 * Convert a sysclock snapshot into a struct bintime based on the specified
 * clock source and flags.
 */
 - yep thats it:  from a stored state, read out the FLAG-desired from of Abs timestamp in bintime
 - flags dropped:
 		- bintime only, not (nano,micro)  [ these are slower ]
		- fast [get]: have no choice, stored state has already set this choice
	   - why limit these options?  why doesnt  sysclock_getsnapshot  just store it all and
		  allow full flexibility?  just store a copy of th and ffth for gods sake and then use all
		  the existing [get]{bin,nano,micro}[up]time_fromclock   ??
		   - fast is particularly easy to implement, is the cost of tc_delta really so high?
			  also save time by avoiding subsequent multiplications etc.
			- only need bin because only called in bpf?  and elsewhere can use other fns?
			  bpf used to return a tval via microtime using  bpf_hdr ,  but even bpf_ts is not quite bt
 
sysclock_snap2bintime(struct sysclock_snap *cs, struct bintime *bt,int whichclock, uint32_t flags)
	 - used in bpf_{tap,mtap}, so important
	 - internally, FF branch doesnt use ffclock_abstime I think because input is in
	   form of a sysclock_snap, so it just doesnt work out.  This is customized to give maximum
		flexibility subject to some basic need for performance.
	 - note that even though cs.ffcount is available, when FFclock is read this is not used,
	   instead the usual  tick_time + Delta  is, because that is the way kernel FF reading is setup
	 - BUG?    			bintime_addx(bt, cs->fb_info.th_scale * cs->delta);
	 in  tc_windup has to use mask, and eat up seconds, it overflow not an issue uuu






==========================================================================================




tc_init 
- not used here, used to init actual counters in hardware specific code, see below
- does not init from scratch, assumes tc_{frequency,counter_mask,quality} already there.
- if new counter better, will change the global counter, ie point "timecounter" to it

tc_setclock
- used in settime()  in kern_time.c
          inittodr() in  subr_rtc.c
- boottime[bin] is the UTC time at which boot was deemed to happen, so 
  nominally need  microtime = microuptime + boottime        and
                    bintime =   binuptime + boottimebin
- input is a new UTC (I think), so fn aims to set newboottime = newUTC - stillvaliduptime 
- input is in timespec, but tries to do calculations in bintime, yet reports using
  timespec - all a bit all over the place
- note bef=before, aft=after
- BUG:  the line bintime_add(&bt2, &boottimebin);  appears to do nothing, bt2 not used again.
      should be  bintime_add(&bt, &boottimebin) ?



tc_windup [static]
- called by tc_setclock, tc_ticktock, inittimecounter in this file only
  to that last stored in timehands (at last tick).  
- core operation: get a delta from the current timehands (tho) and incorporate into new one (th)

- delta calling and processing
  - NOTE: delta is based on the `CURrent` timecounter, meaning that used at Last tick!
    [accessing via the new th->th_counter  not tho->th_counter,  but same thing straight after copy]
    Confusing, because "timecounter" is in fact the current timecounter in the true sense of currently/most-recently selected.
    Here the current truetime of this timestamp is at the location of the bcopy line..
  - what changes the timecounter? here it is checked if "timecounter" different, but
    captured ncount not used until change timecounter code lower down
	 Reading the new tc ASAP is  the priority because if that has changed, the the FFclock is going to go all screwy anyway, and in fact the raw FF read has
	 already been determined once delta is anyway, so no rush.

  - timecounter change overview:
      Ok I think this is the logic.  The goal is to prepare the data for the next timehands (th).
	   The fields  th_{,,} relate to the absolute time at the start of the tick. These must be
		calculated using the data from the old tick (tho), and hence must use the timecounter
		used there, and hence "delta", which will bridge the start of the old up to the start of this one.
		Once that is done the fields related to the new tc can be set, namely
		th_counter, th_offset_count, th_scale .    The th_adjustment is NTP black magic out of our hands.

  - tc overflow handling:
    All calls to read the counter use   tc_counter_mask   and  th_offset_count:
	 in tc_delta(timehands)  itself:
	      tc->tc_get_timecount(tc) - th->th_offset_count) &  tc->tc_counter_mask
	 in the workhorse FB clock read fn  fbclock_binuptime  via tc_delta :
	      *bt = th->th_offset + th->th_scale * tc_delta(th)        // mask in tc_delta
    in tc_windup:
	    th->th_offset_count + tc_delta(th)         					   // mask in tc_delta
		 th->th_offset_count &= th->th_counter->tc_counter_mask;    // and again explicitly

    The comments in the mask defn say it just masks off unused bits but it must
	 also somehow perform a mod to work across a wrap.  TODO:  understand this in detail
	 Note:  the hardware counter may have b<32 bits.  Then the mask is particularly needed,
	 since overflow would not natively vanish, eg in th->th_offset_count + tc_delta(th),
	 so need to kill the overflowed bits to effectively emulated an overflowed counter
	 of b bits.  This part clear, the key part is how tc_delta works, all is based on that.

  - test=="just overflow" since first test=="delta over 1/2 sec" which would set 
    the MSB unless it overflows. Action makes sense because the 2nd arg of bintime_addx will 
    overflow (is cast to the type of scale), leaving only the fraction.

- PPS stuff seems unused..
	- compact use of tc_poll_pps function pointer (if fn defined, then call it)
	- does not return anything - I think it advances the PPS counter if appropriate,
	  as opposed to just reading it, which tc_get_timecount does

- NTP second processing
	- ntp_update_second [Mills]:  is in kern/kern_ntptime.c, into the belly of the beast..
    - NTP second processing uses NTP stuff plus black magic, and seems to be an algo 
      patch on top of NTPs second control algo..
      boottimebin can be impacted - impact on FFclock? or will the high level time calling
      routines bypass all of this?  they should
	  
- Changing timecounters
	- doesnt do that much, update all timehand fields, but does trigger ffclock_change_tc()
    - tc_min_ticktock_freq (accessed here and in kern_clocksource.c via timetc.h, see below)
	   where it is used in this fn only:
		/*
  250  * Schedule binuptime of the next event on current CPU.
  251  */
  252 static void
  253 getnextcpuevent(struct bintime *event, int idle)
  
      -- set here from default global init of 1.  
	  -- This is not counter freq but of rate of calls to tc_windup need to safely 
		 process deltas before round off, or sth like that.  (want to avoid a 2nd wrap, as code
		 handles just one)
         Hence I guess "ticktock" means a tick''s  worth of counter increments.
		 Understand this later: seems to set to be the number of (in fact 3 )
         times the counter can be incremented once-per-second before it is full.
	    It should be resetting tc_tick if this is its goal, but doesnt seem to do...
		 
- scaling factor [ in fact period estimate ]
	-- error in th_adjustment in fact around -3.3PPM  // old comment based on detail look..
	-- here using the same trick as in ffclock_reset_clock and elsewhere to 
	   set period via 1sec/freq. Trick requires *2 at end, hence must *1024 not 512.
		BUG?  check if this is fixed already, if not, ask author, good way to get respectful attention
	-- additive nature of th_adjustment defined by NTPs PPL.
	-- why outside of changed timecounter test? because if changed, its ntpds job
	   to work out what th_adjustment would be (presumably +-5000PPM initially..)
	   Makes sense, tc_windup should windup and set, not estimate or reset daemons, 
	   but division of labour seems unclear..

- note generation trick, cycles ogin (note cycle much longer that the 10 timehands!, 
  so wont get wrapping ambiguity) but skips over th_generation=0 as that is a 
  special reserved value.

- update of convenience globals  time_{second,uptime}
  - why have this explicit sysclock_active code?  why not just use the generic
    getbin{,up}time.sec  ?   I guess that would use the old tick (ie tho data), whereas here
	 we are creating the New th data .  Note however that the FF side would work, since ffwindup has already been called and returned.  Hangon, th has now been finished as well, except for the final timehands = th.
	 - if called them after this instead, then the windup may hang a bit .. anyway the gen code would be a waste of time, just slow down the return of windup with should be high priority.
  - Aah, this is an example of  the FB variables in timehands still being updated even if FFclock, but the globals time_{second,uptime} are apparently supposed to be set according to the chosen clock..  These are used in kern_ntptime.c:  but is that a ntpd function?
	 
	 TODO:  see if time_{second,uptime} still being used in CURRENT, maybe the problem can go away..
				

/********* PPS-API *************/

Each fn has FF components, but only ifdef FFCLOCK
Related API functions defined in timepps.h, with FF parts UNprotected by FFCLOCK
Seems to do minimum to make it work/coherent with existing code (obey standard)

pps_ioctl
-

pps_init
-

pps_capture
-

pps_event
-


/********* ! PPS-API *************/

NOTE: ''tick'' refers to a point on the HZ hardclock interrupt grid

static int tc_tick;
SYSCTL_INT(_kern_timecounter, OID_AUTO, tick, CTLFLAG_RD, &tc_tick, 0,
		   "Approximate number of hardclock ticks in a millisecond");
- message should not be hardwired to a ms  ** no actually it is good I think
- tc_tick thought of as a timeout for when tc_windup should be called
																				

tc_ticktock
- call tc_windup if interrupt cycle timeout exceeded
- called by hardclock in kern_clock.c only, (accessed via timetc.h)
  where interrupt scheduling is organised
- note count is static
- is this THE code that actually makes tc_windup be called once per interrupt cycle?



inittimecounter [static]
- recall hz is the value in Hertz of the interrupt cycle
	- traditional value of hz = 100 Hertz  = 10ms tick period
	- modern                    1000       =  1ms tick period
- hz/1000 = #cycles per ms
- units of tc_tick is ~ interrupt cycle in kilohertz
- 	p = (tc_tick * 1000000) / hz  = 1000 + ?;   // units: cycles per ms/ cycles per s = x1000
- initializes timeout, warms up counter, calls tc_windup
- does comment make sense? if hardware counter ticking slower than hz, then
  reading it using non-get fns wont help, the counter will not have changed.
 YES:  this is not about the tc, `hardclock` refers to the timer interrupt mechanism.
  What they are saying is that if you want higher resolution, interpolate properly to
  a timestamp within a tick, dont just shrink the grid and use get = FAST
- counter values cast to void instead of keeping them - performs some kind of "warmup"
  which is more than keeping a cache hot I guess, but what?
- ** differences here compared to 10.1, involving new fns


SYSINIT(timecounter, SI_SUB_CLOCKS, SI_ORDER_SECOND, inittimecounter, NULL);
- some kind of driver initialization macro, low level, defined in bsd_kernel.h
- this is the only place where inittimecounter called


/* Cpu tick handling -------------------------------------------------*/
- this is for TSC only, not for a wallclock time type clock, but as the basis
  of the whole interrupt cycle timing, and perhaps all kernel timers..
- looks like set_cputicker is used by machine dependent code, after which 
  this file controls the data behind these fns. cpu_tick_calibration
  used to fix up suspend etc events.
- these dont refer to each other hardly at all, seems to be fns mainly used
  elsewhere, probably independent of main time keeping fns and system clock

static int		cpu_tick_variable;   // logical:  "is a ticker whose rate varies"
static uint64_t	cpu_tick_frequency;  // in Hertz  Not clear if high accuracy assumed/needed
										
tc_cpu_ticks [static]
- implements a 64 bit cumulative version of current counter!! [ not necc. TSC]
- with this, who needs FFtimehands?  assuming called often enough, 
  but only used in these CPU tick fns.
- could be just a way to get back the original 64 bits if forced to work with 
  stupid timecounter structure.  
  I guess changing timecounter definition was too big a step first up
- is this in fact just used as a fall back simple counter for init purposes?  
  or one among many, or the preferred/default??  where/why are others defined?

cpu_tick_calibration
- ensures dont calibrate more often than once per 16 seconds  ( 16 = 0xf )
- recall time_uptime is just the second component
- condition reads: "if second different from last time, AND at multiple of 16 seconds" 
- controls access to cpu_tick_calibration which is static here

cpu_tick_calibrate [static]
- main goal is to set cpu_tick_frequency using Delta(TSC)/Delta(t), but only 
  does so if freq Increased, which makes it a poor tracker of drift, let alone
  of a changed counter
- result depends on accuracy and consistency of getbinuptime and cpu_ticks, if
  the counter changes, then these will be crap unless reset nicely before this is called
- If called to reset, marks this fact via t_last.sec=0
  Next normal call, nothing fancy, just record last getbinuptime and number of ticks 
  (assuming valid..)
  Subsequent normal calls, try to recalibrate based on last time and this time. 
- headroom is about when the 44 bits available in the shifted c_delta would overflow:
  would need over 1.099 terahertz
- divi arithmetic:  takes lower 12 bits of seconds, and highest 20 bits of the fraction,
  and puts them into a single 32bit form of time. This can count up to 4096 sec 
  (well over the ~16 needed), and include a bit more than 1mus of precision. 
  Need to remember where the fraction starts..
- freq estimate: align with fractional part of divi and divide, 
  effectively num and dem both x 2^20, so result is in Hz

set_cputicker
- just sets three external variables in this file
- is the only place where cpu_tick_variable is set, but value passed in when 
  fn called from machine dependent code.
- again, is the default choice to tc ticker because is convenient default, or rough init?
- note cpu_ticks defined lower, but already declared extern in system.h

cpu_tickrate
- if using tc ticker, use corresponding freq, otherwise return one used
- but why would they not always agree? is this just faster if tc used?

cputick2usec
- in each case returns tick*1e6/freq [like TSC*phat*1e6] will trust the int arithmetic


cpu_tick_f	*cpu_ticks = tc_cpu_ticks;
- From systm.h:  319 typedef uint64_t (cpu_tick_f)(void);
- so this initializes cpu_ticks to tc_cpu_ticks() defined above, a const evaluated at compile time
- tc_cpu_ticks is local to this file, cpu_ticks is used in many places, eg in
  systm.h:  321 extern cpu_tick_f *cpu_ticks;   and in cpu.h
  I guess it is for backwards compatibility, to make existing cpu_ticks() calls be
  implemented using this tc variant. 
  Or, because this is only one possible "cpu_ticker", perhaps viewed as a fallback.
  



/********* bpf.{h,c} ***********/
#include "opt_ffclock.h"
#include <sys/timeffc.h>

=============== NEW summary including differences to newer version

bpf.h
 Protected
 		# define BP 			/* Feed-forward counter accessor */   cant find any use of this
 Unprotected
 		New BPF_T_{FFCOUNTER,*CLOCK*} and derived macros
		New ffcounter members of  struct bpf_{x,}hdr


bpf.c
 Protected:
 		catchpacket	(declaration and defn)	// copying ffcount into hdr, new parameter
 Protected from:
		bpfioctl  									// disable FF-related checks on timestamp format
 Partially and UnProtected:
		bpf_{tap,mtap,mtap2} 					// new Universal sysclock timestamping
 Protected by !BURN_BRIDGES
		struct bpf_hdr32							// 32-bit specific version of ffcounter enhanced bpf_hdr
 Unprotected
 		CTASSERT statement 						// keep eye on minimal size of bpf timestamp fields
		#define SET_CLOCKCFG_FLAGS				// macro `function' processing CLOCK related flags



bpf_tap:  [comparing 9.0 ]
	Generic aspects
	- doesnt know about pkt direction, this is just about tapping off the timestamp
	- hard to say since so many changes !
	
	FF aspects
	- lots of new variables:  cs, clockflags, pktlen, slen, whichclock, tstype
		tstype is there to work with the new  bpf_if.tstype  member
	- BPF_TSTAMP_ code to work out  interface-level TS options
		- FAST is an interface level attribute, since if even a single descriptor wanted
		  non-fast, then you would have to take a non-fast snapshot, so none would be fast
	   - makes sense to move from per-d BPF_T_FAST to this for perf gain
	- sysclock_getsnapshot   // get complete FB/FF state snapshot prior to looking at descriptors
   - loop over all descriptors attached to interface, for each:
		- ++d->bd_rcount;  // count pkt
		- take care of jitter if enabled, whatever that is . Note slen = snaplen
		- if pass filter
				++d->bd_fcount;  // count pkts that pass
				/* Put timestamp in bt based on BPF_T_* flags in d->bd_tstamp set by deamon via pcap */
				  - previously done with bpf_gettime using
				     - BPF_TSTAMP_{NONE,NORMAL,EXTERN}
					  - returns:  external, or [get]binuptime depending on BPF_TSTAMP_NORMAL
				if d->bd_tstamp == BPF_T_FFCOUNTER
					 bcopy(&cs.ffcount, &bt, sizeof(ffcounter)); // grab ffcounter, put in sec or frac?
				// no masking here to allow  FFCOUNTER and NANO to mix...
				// easy to fix, just  if  LHS >= BPF_T_FFCOUNTER
				// This is KV=2 logic, if BPF_T_FFCOUNTER, shove in a bt and move to catchpacket!
				// nothing else.
				else  // or by default if  !FFCOUNT
				 if pass same if-level TS tests as before (ie if have snapshot)
				 {
				    SET_CLOCKCFG_FLAGS(d->bd_stamp, active clock, whichclock=-1, clockflags=[]
					 		// tries to set flags to sensible FB values for next call
							// is always FB! not the sysclock,  hence ignores LERP and LEAP
							// takes care of UPTIME
					 sysclock_snap2bintime(&cs, &bt, whichclock, clockflags); // return a FB TS
				 }
				 // at this point should have dealt with clock, LERP etc flags
				 // only thing left is FORMAT
				catchpacket(d, pkt, pktlen, slen,bpf_append_bytes, &bt, &cs.ffcount);
					// skip if not received (MAC only)
				   // call FF with extra ffcounter arg if FFCLOCK, else standard
		  else
		     do nothing
			  

#define	SET_CLOCKCFG_FLAGS(tstype, active, clock, flags) do {		\
	(flags) = 0;							\
	(clock) = SYSCLOCK_FBCK;					\
	if ((tstype) & BPF_T_MONOTONIC)					\
		(flags) |= FBCLOCK_UPTIME;				\
} while (0)

Called in bpf_tap in a scenario where tsmode is not BPF_T_FFCOUNTER,  with:
  SET_CLOCKCFG_FLAGS(d->bd_stamp, active clock, whichclock=-1, clockflags=[] )
  - initializes clockflags to 0
  - sets whichclock = SYSCLOCK_FBCK regardless of active clock !
  - ignores active clock
  - adds FBCLOCK_UPTIME into clockflags  if  BPF_T_MONOTONIC flag set in bd_stamp
     - Yes!  they call the uptime clock (the default FB clock in timehands) a monotonic clock
	    because it doesnt have leapsecond jumps.  To get UTC, have to add boottimebin.
       


static void catchpacket
  - struct bpf_hdr   hdr_old;
  - struct bpf_hdr32 hdr32_old;    // both have extra FFcount field, so this is KV=3 in that sense
  - BURN_BRIDGES predates FF changes.   Seems to be a directive to drop
    anything involving  struct bpf_hdr hdr_old;  that is 50% of the function!
	 Was introduced in 9.0 it seems.
  - lots of stuff worried about buffer overflow and capture size
  
  - next step is to fill the relevant hdr structure
		- tstype = d->bd_tstamp;    // descriptor-level tstype, obtained from calling program!
		- Code has bpf_hdr branches protected by BURN_BRIDGES
			 This executes in the case tstype = BPF_T_MICROTIME, because only then is
			 the ts field in hdr_old or hdr32_old, namely timeval, compatible.
			 (Or in the BPF_T_NONE case, where it is sufficient)
			 There are two flavours with equivalent code, one for explicitly 32bit streams
			 (detected by d->bd_compat32 and protected by COMPAT_FREEBSD32, even in 8.0),
			 and the normal one.
			 The code is sequential with a goto copy: to skip the one not needed.
			 Both branches have equivalent FFclock protected code:
			 hdr{32}_old.bh_ffcounter = *ffcount;
		  NOTE:  code would be confused if BPF_T_MICROTIME mixed with a FFCOUNT flag
	   - Other tstype values:  // or if BURN_BRIDGES
		    - work with struct bpf_xhdr hdr;
			 - hdr.bh_ffcounter = *ffcount;  // good, always use passed ffcount
			 - if (tstype & BPF_T_FFCOUNTER)   // BPF_T_FFCOUNTER is its own mask
			 		bcopy(bt, &hdr.bh_tstamp, sizeof(ffcounter));// bad, matches bpf_tap''s KV=2 crap!
		              // no other tstype fields acted on, which makes sense under KV=2
				else  // or if !FFCLOCK
			      bpf_bintime2ts(bt, &hdr.bh_tstamp, tstype);  // obeys FORMAT, BPF_T_MONOTONIC only
					// note if BURN_BRIDGES, then BPF_T_MICROTIME also dealt with here
					- BUG, this acts on BPF_T_MONOTONIC, but sysclock_snap2bintime has already
					  done this in bpf_tap, so will do it twice .
		
  - finally copies pkt data into buffer.  Timestamp(s) is there is header fields.

- In short:  this is KV=3 in terms of structures, but functionality implements KV=2,
  partly because just not trying, partly because the BPF_T_ language is not rich enough to
  allow FFCOUNT to be requested and also to specify a standard FORMAT choice for the
  normal timestamp, unless we abuse that language.
  Could be easily fixed I think by something like  (need to fix bpf_tap first)
    if (tstype & BPF_T_FFCOUNTER) {
		  hdr.bh_ffcounter = *ffcount;   // if want FFcounter, put it in
		  standardformat = BPF_T_FORMAT(tstype) & BPF_T_FFCOUNTER;  // mask out BPF_T_FFCOUNTER
	 }
	 bpf_bintime2ts(bt, &hdr.bh_tstamp, standardformat);  // convert bt as directed regardless
	 																		// assumes bt is the desired true ts




bpf_bintime2ts(struct bintime *bt, struct bpf_ts *ts, int tstype)
  - acts on  BPF_T_MONOTONIC, only place used in entire bpf.c !  makes sense, just add a constant
  - obeys BPF_T_FORMAT(tstype) = BPF_T_{MICRO,NANO,BIN}TIME and calls bintime2time{val,spec}
    to convert, dumping the fraction fields into ts->bt_frac  (not a bintime field in general)
  - functionality the same from 9.0 to current


	- code to skip outgoing duplicates linked to fact now have snapshot for all effectively already?
????

sys/sys/queue.h :
534 #define LIST_FIRST(head)        ((head)->lh_first)
585 #define LIST_NEXT(elm, field)   ((elm)->field.le_next)

535 
536 #define LIST_FOREACH(var, head, field)                                  \
537         for ((var) = LIST_FIRST((head));                                \
538             (var);                                                      \
539             (var) = LIST_NEXT((var), field))

====================================================
# Recall key generic  sys/net/bpfdesc.h  bits
====================================================
Defines:
	bpf_d
	macros for bd state
	eXternal representation of bpf_d : xbpf_d
	#define BPFD_{UN}LOCK(bif)

/* Descriptor associated with each open bpf file. */
struct bpf_d {
  // needed in bpf_tap
   LIST_ENTRY(bpf_d) bd_next;      /* Linked list of descriptors */
   u_int64_t       bd_rcount;      /* number of packets received */
   u_int64_t       bd_fcount;      /* number of packets that matched filter */
   int             bd_direction;   /* select packet direction */
	struct bpf_insn *bd_rfilter;    /* read filter code */
	void            *bd_bfilter;    /* binary filter code */
	int             bd_tstamp;      /* select time stamping function */
	struct mtx      bd_mtx;         /* mutex for this descriptor */
 ....
  // needed in catchpacket
	caddr_t         bd_sbuf;        /* store slot */
   caddr_t         bd_hbuf;        /* hold slot */
   caddr_t         bd_fbuf;        /* free slot */
   int             bd_slen;        /* current length of store buffer */
   int             bd_hlen;        /* current length of hold buffer */
   int             bd_bufsize;     /* absolute length of buffers */
   u_int64_t       bd_dcount;      /* number of packets dropped */
   u_char          bd_state;       /* idle, waiting, or timed out */
   u_char          bd_immediate;   /* true to return on packet arrival */
	u_char          bd_compat32;    /* 32-bit stream on LP64 system */
...
}
====================================================
# Recall key bpf.h bits:
================================

#ifdef FFCLOCK
/* Feed-forward counter accessor */
#define	BP
#endif
 - only eg of a FFCLOCK protection of a .h file, but seems to not be used, placeholder test?


// augment header structure with new counter member
struct bpf_ts {
	bpf_int64	bt_sec;		/* seconds */
	bpf_u_int64	bt_frac;		/* fraction */   // bad name!  this is not bintime
};
 - not bintime!  Goal seems to be a `universal` bpf timestamp format capable of holding the
  frac component of timeval, timespec, as well as bintime  (see bpf_bintime2ts)

struct bpf_xhdr {
	struct bpf_ts	bh_tstamp;	/* time stamp */   // was timeval in old bpf_hdr
	ffcounter	bh_ffcounter;	/* feed-forward counter stamp */
	bpf_u_int32	bh_caplen;	/* length of captured portion */
	bpf_u_int32	bh_datalen;	/* original length of packet */
	u_short		bh_hdrlen;	/* length of bpf header (this struct plus alignment padding) */
};

/* Obsolete */
struct bpf_hdr {
	struct timeval	bh_tstamp;	/* time stamp */
	ffcounter	bh_ffcounter;	/* feed-forward counter stamp */
	bpf_u_int32	bh_caplen;	/* length of captured portion */
	bpf_u_int32	bh_datalen;	/* original length of packet */
	u_short		bh_hdrlen;	/* length of bpf header (this struct plus alignment padding) */
};
  - these three all in current BSD header !!  including the Obsolete comment.
  - seems to be just an upgrade from timeval
  - the actual FF innovation is the inclusion of bh_ffcounter  in BOTH the
    new and Obsolete forms

/*
 * Descriptor associated with each attached hardware interface. IF=InterFace
 */
struct bpf_if {
	LIST_ENTRY(bpf_if)	bif_next;	/* list of all interfaces */
	LIST_HEAD(, bpf_d)	bif_dlist;	/* descriptor list */
	u_int bif_dlt;							/* link layer type */
	u_int bif_hdrlen;						/* length of link header */
	struct ifnet *bif_ifp;				/* corresponding interface */
	struct mtx	bif_mtx;					/* mutex for interface */
	struct sysctl_oid *tscfgoid;		/* timestamp sysctl oid for interface */
	int tstype;								/* interface-level timestamp attributes */
};                                  // BPF_TSTAMP_{DEFAULT,NONE,FAST,NORMAL,EXTERNAL}
Differences compared to 9.0 = 9.1
 - tscfgoid, tstype missing, otherwise is the same
 - every other version is different !  and is moved into bpf.c in BSD11



/* Time stamping functions */
// timestamp FORMAT flags	[ only FFCOUNTER new ]
#define	BPF_T_{{MICRO,NANO,BIN}TIME},NONE,FFCOUNTER}		0x000{0 1 2 3 4}  // should call raw?
#define	BPF_T_FORMAT_MAX	0x0004	  // records largest format value
#define	BPF_T_FORMAT_MASK	0x0007   // 0000 0000 0000 0111  isolates bits containing format
#define	BPF_T_NORMAL		0x0000   // points to MICRO & not FFCOUNTER as the default FORMAT?

BUG:  stupidly mix FFCOUNTER with normal timestamp formats, so cant have both, but these are always
separate anyway (except under KV=2), so this means you cant specify the format of the timestamp !
and yet, one will be returned!  what will its format be??
Hmm, unless, given max is 4 but mask is 7, values 5,6,7 can be interpreted as FORMAT combinations.
In fact FFCOUNTER = 0 0 0 0100, so it can be mixed safely with the other 4.

// clock-type mono FLAG flags  [ old ]
#define	BPF_T_MONOTONIC	0x0100   // ** means want uptime clock, as UTC is not monotonic
										     // in 9.0, had T_FAST as well
#define	BPF_T_FLAG_MASK	0x0100   // 0000 0001 0000 0000		isolates mono flag trivially
// CLOCK paradigm flags  [ new ]
#define	BPF_T_SYSCLOCK		0x0000	  // how used/
#define	BPF_T_FBCLOCK		0x1000
#define	BPF_T_FFCLOCK		0x2000
#define	BPF_T_CLOCK_MAX	0x2000   // largest clock flag value
#define	BPF_T_CLOCK_MASK	0x3000	  //  0011 0000 0000 0000 0000  isolates paradigm flags

// Use masks to extract passed  FORMAT, FLAG, CLOCK
#define	BPF_T_FORMAT(t)	((t) & BPF_T_FORMAT_MASK)
#define	BPF_T_FLAG(t)		((t) & BPF_T_FLAG_MASK)
#define	BPF_T_CLOCK(t)		((t) & BPF_T_CLOCK_MASK)			// new

// (FORMAT,FLAG,CLOCK) combination valid if
#define	BPF_T_VALID(t)		\
    ((t) == BPF_T_NONE || (t) == BPF_T_FFCOUNTER || \   // (NONE or FFCLOCK,notMONO,SYSCLOCK) or
    (BPF_T_FORMAT(t) <= BPF_T_BINTIME && BPF_T_CLOCK(t) <= BPF_T_CLOCK_MAX &&
    ((t) & ~(BPF_T_FORMAT_MASK | BPF_T_FLAG_MASK | BPF_T_CLOCK_MASK)) == 0))
 // (think: ~mask = forbidden positions)
Meaning:
if FORMAT= NONE or FFCLOCK or
   (FORMAT standard) and (CLOCK is valid) and (no bits in illegal posns)
 - why would an invalid clock or illegal bits be ok if FORMAT=FFCLOCK ?   because in fact
   we abuse those bits but dont declare that here?  Probably because there are many cases,
	but this defn covers what is needed in the code.
 - this classes  BPF_T_NANO | BPF_T_FFCOUNTER  as invalid , since the code does not mix them.

// explicitly named combinations of  (FORMAT,FLAG)  [ old ]
#define	BPF_T_MICROTIME_MONOTONIC	(BPF_T_MICROTIME | BPF_T_MONOTONIC)	// MICRO and MONO
#define	BPF_T_NANOTIME_MONOTONIC	(BPF_T_NANOTIME | BPF_T_MONOTONIC)  // NANO  and MONO
#define	BPF_T_BINTIME_MONOTONIC		(BPF_T_BINTIME | BPF_T_MONOTONIC)	// BIN   and MONO
// explicitly named combinations of  (FORMAT,FLAG,CLOCK)  [ new ]
#define	BPF_T_FBCLOCK_MICROTIME_MONOTONIC (BPF_T_MICROTIME_MONOTONIC | BPF_T_FBCLOCK)
#define	BPF_T_FBCLOCK_NANOTIME_MONOTONIC	 (BPF_T_NANOTIME_MONOTONIC  | BPF_T_FBCLOCK)
#define	BPF_T_FBCLOCK_BINTIME_MONOTONIC	 (BPF_T_BINTIME_MONOTONIC   | BPF_T_FBCLOCK)
#define	BPF_T_FFCLOCK_MICROTIME_MONOTONIC (BPF_T_MICROTIME_MONOTONIC | BPF_T_FFCLOCK)
#define	BPF_T_FFCLOCK_NANOTIME_MONOTONIC	 (BPF_T_NANOTIME_MONOTONIC  | BPF_T_FFCLOCK)
#define	BPF_T_FFCLOCK_BINTIME_MONOTONIC   (BPF_T_BINTIME_MONOTONIC   | BPF_T_FFCLOCK)
 - dont seem to need any of these
 - in some kernels have combinations with FAST as well, but again dont need, hardly ever used


Differences compared to 9.0 up to 11 and current
  FORMAT:  no FFCOUNTER
  FLAG:    had BPF_T_FAST and still has  // why not now?
  CLOCK:	  nothing
  VALID:	  FORMAT= NONE or  or  (FORMAT not NONE) and (no bits in illegal posns)
 Combinations:  all (FORMAT,FLAG) combinations
  bpf_xhdr exists
Differences compared to 8:   none of the above existed



==========================================================================
# Record of all relevant changes in bpf subsystem affecting 9.0 --> 11.2
==========================================================================


#=============== In bpf.h

#====== ADDED
411 struct bpf_if_ext {
  412         LIST_ENTRY(bpf_if)      bif_next;       /* list of all interfaces */
  413         LIST_HEAD(, bpf_d)      bif_dlist;      /* descriptor list */
  414 };
  
  
#====== REMOVED
struct bpf_if;    // this is now in bpf.c  , leaving only the bpf_if_ext publically visible
 
was:
struct bpf_if {
	LIST_ENTRY(bpf_if)	bif_next;	/* list of all interfaces */
	LIST_HEAD(, bpf_d)	bif_dlist;	/* descriptor list */
	u_int bif_dlt;				/* link layer type */
	u_int bif_hdrlen;		/* length of link header */
	struct ifnet *bif_ifp;		/* corresponding interface */
	struct mtx	bif_mtx;	/* mutex for interface */
	struct sysctl_oid *tscfgoid;	/* timestamp sysctl oid for interface */
	int tstype;			/* timestamp setting for interface */
};
 
#================ In bpf.c

#====== ADDED

struct bpf_if {   //  was in bpf.h
  100 #define bif_next        bif_ext.bif_next   // emulated older member name
  101 #define bif_dlist       bif_ext.bif_dlist  //  " 			"
  102         struct bpf_if_ext bif_ext;      /* public members */   // hide some queue stuff
  106         struct rwlock   bif_lock;       /* interface lock */   // was bif_mtx
  107         LIST_HEAD(, bpf_d) bif_wlist;   /* writer-only list */
  108         int             bif_flags;      /* Interface flags */
   // [bpfdesc.h]:  takes values in   BPFIF_FLAG_DYING  BPF_{R,RUN,W,WUN}LOCK
  109         struct bpf_if   **bif_bpf;      /* Pointer to pointer to us */




#====== REMOVED



#====== SAME [as 9.0 without modifications ]
...
#define	BPF_TSTAMP_{NONE,FAST,NORMAL,EXTERN} = {0,1,2,3}
  bpf_{ts_quality}   // uses {NONE,FAST,NORMAL},
	 - translates from BPF_T_{NONE,FAST,'other'} from d->bd_tstamp) to BPF_TSTAMP_{NONE,FAST,NORMAL}
	 - this is from d-level to if-level in a sense
	 - more importantly, translates from _T_ language allowing  daemon<-->FF communication, to
	   what needs to be done internally
  bpf_gettime			// uses {NONE,    ,NORMAL,EXTERN}   acts on FAST implicitly
    - calls and returns  bpf_ts_quality, or EXTERN
	 - if BPF_TSTAMP_NORMAL,  call [get]binuptime(bt)
	 	- effectively, implements  if BPF_T_FAST then  get]binuptime, but has to work
		  through a middleman  BPF_TSTAMP_FAST to do it !! seems pointless
  bpf_{tap,mtap{2}}	// uses {NONE},  calls  bpf_{ts_quality,gettime}
#undef straight after  bpf_mtap2






====================================================
# Recall modified bpf.c bits:
================================

#====== ADDED to 9.0, not incorporated

Externs used in bpf.c
 - static int bpf_default_tstype = BPF_TSTAMP_NORMAL;   // initialising system wide default

// structure replacing #defines, added very near top of file,  scope entire file
static const char *bpfiftstypes[] = {   // ie possible values for new field bpf_if.tstype
	 #define	BPF_TSTAMP_DEFAULT	0  // [new], means `want default', ie set to bpf_default_tstype
	 #define	BPF_TSTAMP_NONE		1
	 #define	BPF_TSTAMP_FAST		2
	 #define	BPF_TSTAMP_NORMAL		3
	 #define	BPF_TSTAMP_EXTERNAL	4  // was _EXTERN
};
 - these types are interface (if) types for control of the bpf_if.tstype
 field, the BPF_T_  timestamp types are for per-descriptor control
 - they are for internal use only, not visible outside, in particular by daemon
 - used in:
 		bpf_{tap,mtap{2}}
		bpf_tscfg_sysctl_handler		// the big one, manage default settings
		bpfattach2							// initialization, including new bpf_if members
	 

/* 
 * Show or change the per bpf_if or system wide default timestamp configuration 
 */
static int bpf_tscfg_sysctl_handler
  - entire sysctl addition is new, reveals intent
  	- may be ill-considered now given COMMENT on the fact that a single interface may
	  have many bpf_if attached to it ??
  - something like:
      - if ask to reset system default, then do that
		- else set his bfp_if  tstype  to input value
		
 
/* Attach an interface to bpf.  ifp is a pointer to the structure defining the
   interface to be attached, dlt is the link layer type, ... */
void bpfattach2(struct ifnet *ifp, u_int dlt
	 - sysctl PROC added here: ""Interface BPF timestamp configuration"
	 - bpf_if.tscfgoid   used
	 - bp->tstype = BPF_TSTAMP_DEFAULT   init

void bpfdetach(struct ifnet *ifp)



bpf interface tree, with new node _net_bpf_tscfg, is
_net
  -> _net_bpf		"bpf sysctl"
      - maxinsns 			"Maximum bpf program instructions"
  		- zerocopy_enable	"Enable new zero-copy BPF buffer sessions"
		-> _net_bpf_stats					"bpf statistics portal"
		-> _net_bpf_tscfg [static] 	"Per-interface timestamp configuration"
	       bpf_tscfg_sysctl_handler	[PROC]	"Per-interface system wide default timestamp configuration"
			   children: bpf_if.tscfgoid 			"Interface BPF timestamp configuration"
		




#====== REMOVED
 bpf_ts_quality(int tstype)
 bpf_gettime(struct bintime *bt, int tstype, struct mbuf *m)
 - but if remove these, then case strong to kill the _TSTAMP_ themselves, instead of
   strengthening them !!



================================================================================

#ifndef BURN_BRIDGES
/*
 * 32-bit version of structure prepended to each packet.  We use this header
 * instead of the standard one for 32-bit streams.  We mark the a stream as
 * 32-bit the first time we see a 32-bit compat ioctl request.
 */
struct bpf_hdr32 {
	struct timeval32 bh_tstamp;	/* time stamp */
	ffcounter	bh_ffcounter;	/* feed-forward counter stamp */
	uint32_t	bh_caplen;	/* length of captured portion */
	uint32_t	bh_datalen;	/* original length of packet */
	uint16_t	bh_hdrlen;	/* length of bpf header (this struct
					   plus alignment padding) */
};
#endif /* !BURN_BRIDGES */
 - 32 bit version of the Obsolete bpf_hdr  , with FFcounter enhancement
TODO:  ask Peter about this guard macro


/*
 * Safety belt to ensure ABI of structs bpf_hdr32, bpf_hdr and bpf_xhdr are
 * preserved for use with FFCLOCK, which changes the stamp field in the
 * structs to allow storing a regular time stamp or ffcounter stamp.
 */
CTASSERT(sizeof(struct bpf_ts) >= sizeof(ffcounter) &&
    sizeof(struct bintime) >= sizeof(ffcounter));

..

#define	SET_CLOCKCFG_FLAGS(tstype, active, clock, flags) do {		\
	(flags) = 0;							\
	(clock) = SYSCLOCK_FBCK;					\
	if ((tstype) & BPF_T_MONOTONIC)					\
		(flags) |= FBCLOCK_UPTIME;				\
} while (0)

Called in bpf_tap in a scenario where tsmode is not BPF_T_FFCOUNTER,  with:
  SET_CLOCKCFG_FLAGS(d->bd_stamp, active clock, whichclock=-1, clockflags=[] )
  - wierd,  whichclock is just set to SYSCLOCK_FBCK regardless of active clock,
    and sysclock_snap2bintime  therefore uses FB regardless !


//
static	int
bpfioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flags,  ... )
...
	switch (cmd) {

		 case BIOCSTSTAMP:
			 {
				 u_int	func;

				 func = *(u_int *)addr;
	 #ifndef FFCLOCK
				 if (BPF_T_FORMAT(func) == BPF_T_FFCOUNTER ||
					  BPF_T_CLOCK(func) != BPF_T_SYSCLOCK) {
					 error = EINVAL;
				 } else
	 #endif
				 if (BPF_T_VALID(func))
					 d->bd_tstamp = func;
				 else
					 error = EINVAL;
			 }
			 break;







/**********************/

																		
																																					
/****  NEW code since our version 0.4: ****/
sysctl_fast_gettime()

tc_fill_vdso_timehands()

tc_fill_vdso_timehands32()																																														
						
						
/** bsd_kernel.h **/
   #define bcopy(src, dst, len)    memcpy((dst), (src), (len))
   - memcpy is fast, is thread safe, but is Not (in general) atomic
	- in linux, I read that "memcpy is the standard, bcopy is a legacy BSD function."
   																																																																													
/** bsd_kernel.h **/

  - 192 typedef unsigned long size_t;  // so true for all architectures? aah but 64 bit?
    - hang on here "unsigned long" is the alias!!
	- have macro to do the same thing in malloc.h: #define size_t unsigned long
    - would be good to ask expert about this  Peter?
	
  -  typedef __size_t        size_t;   declared in many files, eg types.h
																																																																																																	
					 
										
/** http://fxr.watson.org/fxr/source/sys/errno.h#L73 **/
- defn of all standard error messages

/** sys/sys/_timeval.h **/
struct timeval {
   50         time_t          tv_sec;         /* seconds */
   51         suseconds_t     tv_usec;        /* and microseconds */
   52 };
			

/** sys/sys/time.h **/         http://fxr.watson.org/fxr/source/sys/time.h?v=FREEBSD10
includes _timeval.h

struct bintime {
   54         time_t  sec;
   55         uint64_t frac;
   56 };
- definitions of bintime, bintime manipulation, eg
  #define bintime_clear(a)        ((a)->sec = (a)->frac = 0)
  #define bintime_isset(a)        ((a)->sec || (a)->frac)  // ie, if a!=(0,0)
  #define bintime_cmp(a, b, cmp)                                       \
		  (((a)->sec == (b)->sec) ?                                    \
		   ((a)->frac cmp (b)->frac) :                                 \
		   ((a)->sec cmp (b)->sec) )
    - extends a simple binary comparison operator (like [<,=,>]) to bintimes
  static __inline void bintime_mul(struct bintime *_bt, u_int _x) // result returned via _bt
					  bintime_addx(struct bintime *_bt, uint64_t _x)
					   bintime_add(struct bintime *_bt, const struct bintime *_bt2)
- bintime_{mul,add,sub,addx}	   return result back into first argument  [ for sub: arg1-arg2 ]
- bintime_mul:  2nd argument is not a bintime!
      bintime_mul(struct bintime *_bt, u_int _x)
- bintime2timeval(arg1,arg2)  etc, return result back into second

- conversion fns to and from tval, tspec, and other related manipulations
- interval timer defns, defn of struct clockinfo and different types of clock 
  macros like CLOCK_{MONOTONIC,UPTIME, ...}
- comment block describing [get]{bin,nano,micro}[up]time()  fns
- other fns, including {set,get}timeofday(,)
- commonly referred to variables defined elsewhere, including from kern_tc
	time_t time_second;		// second according to UTC
	time_t time_uptime;		// seconds since boot
	struct bintime boottimebin;
	struct timeval boottime;
			  
			  
/** sys/systm.h **/
 215 void    bcopy(const void *from, void *to, size_t len) __nonnull(1) __nonnull(2);
 216 void    bzero(void *buf, size_t len) __nonnull(1);
 	- null the first len entries of buf
		
 218 void    *memcpy(void *to, const void *from, size_t len) __nonnull(1) __nonnull(2);
 231 int     copyout(const void * __restrict kaddr, void * __restrict udaddr,size_t len) __nonnull(1) __nonnull(2);


/** sys/sysproto.h **/
- automatically collected syscall prototypes  [ no idea how extern declarations handled ]
- ffclock ones are:
731 struct ffclock_getcounter_args {
732         char ffcount_l_[PADL_(ffcounter *)]; ffcounter * ffcount; char ffcount_r_[PADR_(ffcounter *)];
733 };
734 struct ffclock_setestimate_args {
735         char cest_l_[PADL_(struct ffclock_estimate *)]; struct ffclock_estimate * cest; char cest_r_[PADR_(struct ffclock_estimate *)];
736 };
737 struct ffclock_getestimate_args {
738         char cest_l_[PADL_(struct ffclock_estimate *)]; struct ffclock_estimate * cest; char cest_r_[PADR_(struct ffclock_estimate *)];
739 };
1987 int     sys_ffclock_getcounter(struct thread *, struct ffclock_getcounter_args *);
1988 int     sys_ffclock_setestimate(struct thread *, struct ffclock_setestimate_args *);
1989 int     sys_ffclock_getestimate(struct thread *, struct ffclock_getestimate_args *);
2691 #define SYS_AUE_ffclock_getcounter      AUE_NULL
2692 #define SYS_AUE_ffclock_setestimate     AUE_NULL
2693 #define SYS_AUE_ffclock_getestimate     AUE_NULL

- AUE is some kind of Audit system, defined to not be available here (forget it)


										

/********* /usr/src/sys/sys/sysctl.h ***********/
Syscontrol reference

133 #define SYSCTL_HANDLER_ARGS struct sysctl_oid *oidp, void *arg1,        \
134         intptr_t arg2, struct sysctl_req *req

- is arg1 for passing in and arg2 to passing out?  used that way in 
  sysctl_kern_timecounter_{get,freq} in kern_tc

187 #define SYSCTL_IN(r, p, l)      (r->newfunc)(r, p, l)
188 #define SYSCTL_OUT(r, p, l)     (r->oldfunc)(r, p, l) 


// from FreeBSD site, different numbering:
155 /*
 * This describes the access space for a sysctl request.  This is needed
 * so that we can use the interface from the kernel or from user-space.
 */
struct sysctl_req {
	struct thread	*td;		/* used for access checking */
	int		 lock;		/* wiring state */
	void		*oldptr;
	size_t		 oldlen;
	size_t		 oldidx;
	int		(*oldfunc)(struct sysctl_req *, const void *, size_t);
	void		*newptr;
	size_t		 newlen;
	size_t		 newidx;
	int		(*newfunc)(struct sysctl_req *, void *, size_t);
	size_t		 validlen;
	int		 flags;
172 };


/********* /usr/src/sys/sys/sbuf.h ***********/

/* $begin sbuft */
typedef struct {
    int *buf;          /* Buffer array */         
    int n;             /* Maximum number of slots */
    int front;         /* buf[(front+1)%n] is first item */
    int rear;          /* buf[rear%n] is last item */
    sem_t mutex;       /* Protects accesses to buf */
    sem_t slots;       /* Counts available slots */
    sem_t items;       /* Counts available items */
} sbuf_t;
/* $end sbuft */



/********* timetc.h ***********/
-included by kern_tc

19 /*-
20  * `struct timecounter' is the interface between the hardware which implements
21  * a timecounter and the MI code which uses this to keep track of time.
22  *
23  * A timecounter is a binary counter which has two properties:
24  *    * it runs at a fixed, known frequency.
25  *    * it has sufficient bits to not roll over in less than approximately
26  *      max(2 msec, 2/HZ seconds).  (The value 2 here is really 1 + delta,
27  *      for some indeterminate value of delta.)
28  */
29 
30 struct timecounter;
31 typedef u_int timecounter_get_t(struct timecounter *);
- this is the stupidity, the actual counter may be wide, but will be cast to u_int
- problem first propagated by tc_delta, used by tc_windup, that only returns a u_int

32 typedef void timecounter_pps_t(struct timecounter *);
33 
34 struct timecounter {
35         timecounter_get_t       *tc_get_timecount;
36                 /*
37                  * This function reads the counter.  It is not required to
38                  * mask any unimplemented bits out, as long as they are
39                  * constant.
40                  */
41         timecounter_pps_t       *tc_poll_pps;
42                 /*
43                  * This function is optional.  It will be called whenever the
44                  * timecounter is rewound, and is intended to check for PPS
45                  * events.  Normal hardware does not need it but timecounters
46                  * which latch PPS in hardware (like sys/pci/xrpu.c) do.
47                  */
48         u_int                   tc_counter_mask;
49                 /* This mask should mask off any unimplemented bits. */
50         uint64_t                tc_frequency;
51                 /* Frequency of the counter in Hz. */
52         char                    *tc_name;
53                 /* Name of the timecounter. */
54         int                     tc_quality;
55                 /*
56                  * Used to determine if this timecounter is better than
57                  * another timecounter higher means better.  Negative
58                  * means "only use at explicit request".
59                  */
60         u_int                   tc_flags;
61 #define TC_FLAGS_C2STOP         1       /* Timer dies in C2+. */
62 #define TC_FLAGS_SUSPEND_SAFE   2       /*
63                                          * Timer functional across
64                                          * suspend/resume.
65                                          */
66 
67         void                    *tc_priv;
68                 /* Pointer to the timecounter's private parts. */
69         struct timecounter      *tc_next;
70                 /* Pointer to the next timecounter. */
71 };

- I think tc_frequency is the nominal set once only
- the mask is ANDed and I guess is 0000...0011....11111 to remove higher bits that
  the physical counter never sets. 
- Also corresponds to highest value of counter!!
- last member setup up the option to include all available counters in a linked list or
 array, so can cycle around them conveniently.
  
  
73 extern struct timecounter *timecounter;
74 extern int tc_min_ticktock_freq; /*
75                                   * Minimal tc_ticktock() call frequency,
76                                   * required to handle counter wraps.
77                                   */
78 
79 u_int64_t tc_getfrequency(void);
80 void    tc_init(struct timecounter *tc);
81 void    tc_setclock(struct timespec *ts);
82 void    tc_ticktock(int cnt);
83 void    cpu_tick_calibration(void);
84 
85 #ifdef SYSCTL_DECL
86 SYSCTL_DECL(_kern_timecounter);
87 #endif
88 
89 #endif /* !_SYS_TIMETC_H_ */
										

/********* generic_timer.c ***********/

Example of an actual timecounter and reading function for ARM in arm/arm/generic_timer.c
 - functions of type timecounter_pps_t not found in the repository.. 

94 static timecounter_get_t arm_tmr_get_timecount;
95 
96	static struct timecounter arm_tmr_timecount = {
97         .tc_name           = "ARM MPCore Timecounter",
98         .tc_get_timecount  = arm_tmr_get_timecount,
99         .tc_poll_pps       = NULL,
100         .tc_counter_mask   = ~0u,
101         .tc_frequency      = 0,
102         .tc_quality        = 1000,
103 };
...
116 static long
117 get_cntxct(bool physical)
118 {
119         uint64_t val;
120 
121         isb();
122         if (physical)
123                 /* cntpct */
124                 __asm volatile("mrrc p15, 0, %Q0, %R0, c14" : "=r" (val));
125         else
126                 /* cntvct */
127                 __asm volatile("mrrc p15, 1, %Q0, %R0, c14" : "=r" (val));
128 
129         return (val);
130 }
...
193 static unsigned
194 arm_tmr_get_timecount(struct timecounter *tc)
195 {
196 
197         return (get_cntxct(arm_tmr_sc->physical));
198 }
199 
...
322         arm_tmr_timecount.tc_frequency = sc->clkfreq;
323         tc_init(&arm_tmr_timecount); 
- tc_init used here, defined in kern_tc.c



/** /kern/subr_param.c **/

84 int     hz;                             /* system clock's frequency */
85 int     tick;                           /* usec per tick (1000000 / hz) */
86 struct bintime tick_bt;                 /* bintime per tick (1s / hz) */
87 sbintime_t tick_sbt;
- so tick is in fact period in mus?
- in types.h:  189 typedef __int64_t       sbintime_t;



/** /kern/kern_clock.c **/
This is where the interrrupt cycle tick management is done - important.
- opt_ntp.h included here but no sign of FFCLOCK

/** kern_clocksource **/
- I think this is extra stuff to handle sleeping states and maybe multiple cores.
- uses tc_min_ticktock_freq via timetc.h



/** sys/param.h **/
  295 #define nitems(x)       (sizeof((x)) / sizeof((x)[0]))    // use in
 
/** sys/stddef.h **/
	offsetof


