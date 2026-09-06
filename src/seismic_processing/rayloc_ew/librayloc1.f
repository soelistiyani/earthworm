      Subroutine LIBRAY( tokenName, dir_in )
c
c   The main program gets the command line argument and opens input,
c   output, and error files.  It then drives reading the input file,
c   relocating the event, and writing the output file.
c
      implicit none
      save
c
      include 'hypocm.inc'
c
      Character, Intent ( IN ) :: tokenName*(*)
      Character, Intent ( IN ) :: dir_in*(*)
c
      Integer tok_len
      Integer dir_len
c
      character*8 phlst(2)
c
      logical*4 prnt(3)
c
      integer*4 nphase,nexit
c      integer*4 iargc,nargs
      integer*4 min,mout,merr
c
      common /unit/ min,mout,merr
c
      real*4 pi,rad,deg,radius
c
      common /circle/ pi,rad,deg,radius
      integer*4 miter1,miter2,miter3
      real*4 tol1,tol2,tol3,hmax1,hmax2,hmax3,tol,hmax,zmin,zmax,
     1      zshal,zdeep,rchi,roff,utol
c
      common /itcntl/ miter1,miter2,miter3,tol1,tol2,tol3,
     1      hmax1,hmax2,hmax3,tol,hmax,zmin,zmax,
     2      zshal,zdeep,rchi,roff,utol
c
c IGD 06/01/04 Increased static array to allow for pathname
      character*20 modnam
      character*1001 dirname

      common /my_dir/dirname

      integer*4 mlu
c
      common /vmodel/mlu,modnam
c
c Define values carried around in the common block circle
c
      data pi/3.1415926536/, radius/6371.0/
c
c Set the robust regression values
c
      data miter1,miter2,miter3,tol1,tol2,tol3,hmax1,hmax2,hmax3,zmin,
     1 zmax,zshal,zdeep,rchi,utol/10,10,50,3.,3.,2.,500.,300.,30.,0.,
     2 800.,33.,600.,.45,20./
c
c Hard wire the model.
c
      data phlst,prnt/'ALL',' ',3*.false./
c        data modnam/'ak135'/
c
      modnam='ak135'
c      stdin = 5
c      stdout= 6
c
c Define the values needed to convert between radians and degrees that are
c   carried around in the common block circle
c
      rad = pi / 180.0
      deg = 180.0 / pi
c
c   Set the randomizer limit.
c
      roff=.58984375+(rchi-.375)

      tok_len = LEN_TRIM( tokenName )
      dirname = dir_in
      dir_len = LEN_TRIM( dirname )
c
c
c Initialize the reference earth model
c
      call freeunit(mlu)
      call tabin(mlu,dirname(1:dir_len)//'/'//modnam)
      call brnset(1,phlst,prnt)
c
      call freeunit(merr)
      open(merr,file=dirname(1:dir_len)//'/'//
     1	  'RayLocError'//tokenName(1:tok_len)//'.txt',
     2    access='sequential',form='formatted',status='new')
c
c Read in the starting hypocenter, analyst commands, and phase data
c
      call freeunit(min)
      open(min,file=dirname(1:dir_len)//'/'//
     1     'RayLocInput'//tokenName(1:tok_len)//'.txt',
     2     access='sequential',form='formatted',status='old')
      rewind(min)
c
      call input(nphase)
      close(min)
c
c Locate the earthquake
c
      call hypo(nphase)
c
c Calculate the adjustment to the hypocenter parameters and magnitudes,
c  do the final statistics, and write it out.
c
      call freeunit(mout)
      open(mout,file=dirname(1:dir_len)//'/'//
     1     'RayLocOutput'//tokenName(1:tok_len)//'.txt',
     2     access='sequential',form='formatted',status='new')
      call output(nphase)
c
      close(mout)
c   Set the exit flag.
      if(ng.eq.' ') then
        nexit=0
      else if(ng.eq.'o') then
        nexit=1
      else if(ng.eq.'q') then
        nexit=2
      else if(ng.eq.'u') then
        nexit=3
      else if(ng.eq.'s') then
        nexit=4
      else
        write(merr,*)'Unknown status flag (ng = ',ng,').'
        nexit=5
      endif
      close(merr)
      close(mlu)
      return
      end
