!
! © 2013.  Virginia Polytechnic Institute & State University
! 
! This GPU-accelerated code is based on the MPI code supplied by Pengcheng Liu of USBR.
!
PROGRAM disfd 
!
!   is a 4th order viscoelastic Finite-Difference code for the double
!   couple  source. We use 8 pairs of weight and relaxation time 
!   for the modeling of the constant Q.
!
!   This code apply PML to model absorbing boundary conditions
!
!   Node range: kz=1--nztop, jy=1--nytop, ix=1--nxtop
!   kz=1 is the plane of free surface is at. 
!   The artifical boundary are at ix=1, ix=nxtop, jy=1, jy=nytop, 
!   and kz=nztop.  
!
!   In this code it is assumed that positive Z direction is vertical down,
!   the original point of Cartesian coordinates is at (ix=1,jy=1,kz=1),
!   and that the Cartesian coordinates follow 
!       [x,y,z]=[ (ix-1)*gridsp, (jy-1)*gridsp, (kz-1)*gridsp].
!
!    The grid position of these parameters on the staggerd grids system 
!    are as following:
!    vx-- i+1/2, j,     k+1/2 
!    vy-- i,     j+1/2, k+1/2
!    vz-- i,     j,     k
!
!    txx-- i,     j,     k+1/2
!    txy-- i+1/2, j+1/2, k+1/2     
!    txz-- i+1/2, j,     k  
!    tyy-- i,     j,     k+1/2
!    tyz-- i,     j+1/2, k  
!    tzz-- i,     j,     k+1/2
!    
!    Vz, Txz, and Tyz locate on the free surface
!
!  The inputing double couple point source should be given at the point 
!  where Txx, Tyy, and Tzz are located .
!   
!  The inputing Vp, Vs, and roh are the matrial values at the point where 
!  Txx, Tyy, and Tzz are located (i, j, k+1/2).
!
!  If the source function is slip rate, the output is velocity.
!
! Pengcheng Liu 
!=======================================================
!3. split the update_velocity subroutine into the computation and data
!   communication subroutines.
!4. Change the velocity function from fortran to C
!++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 use  mpi
 implicit NONE
 character(len=72)::fileInp
 integer:: nproc_world,myid_world,comm_world,ierr
!
! run_time_b = 1.0 * time8()

 call MPI_INIT( ierr )
 call MPI_COMM_RANK( MPI_COMM_WORLD, myid_world,  ierr )
 call MPI_COMM_SIZE( MPI_COMM_WORLD, nproc_world, ierr )
!
 comm_world = MPI_COMM_WORLD
 if(myid_world == 0) then
   fileInp='disfd.inp'
 else
   fileInp='  '
 endif

 call input_fd_param(myid_world,comm_world,nproc_world,fileInp)
 call MPI_Barrier(comm_world, ierr)

 ! write(*,*) ' after call input_fd_param'
 call run_fd_simul(myid_world)

 call MPI_Barrier(comm_world, ierr)
 call MPI_FINALIZE(ierr)
! write(*, *) "Start", run_time_b, "end", run_time_e, "execution time", run_time_e - run_time_b
 stop
end program disfd
!+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
subroutine input_fd_param(myid_world,comm_world,nproc_world,fname)
 use mpi
 use comput_input_param
!
 implicit NONE
 integer, intent(IN):: comm_world,myid_world,nproc_world
 character(len=*), intent(IN):: fname
 character(len=72):: fileInp,fileMod,fileRec,fileInf
 integer:: group_world,group_worker,nd2_pml,ncount,nrecs,lch,i,k,ierr
 double precision:: RF
!
 lch=min0(70,len_trim(fname))+1
 fileInp=fname(1:lch)
 call read_inp(myid_world,nproc_world,fileInp,fileMod,fileRec, &
               fileInf,fd_out_root,nd2_pml,RF,ierr)
 if (ierr /= 0) then
   call MPI_FINALIZE(ierr)
   stop
 endif
!------------------------------------------------------------------
! Create Working Communicators for the used processors
!------------------------------------------------------------------
 allocate(nfiles(nproc_world))
 call MPI_Comm_group(comm_world,group_world,ierr)
 nproc=nproc_x*nproc_y
 ncount = nproc_world-nproc
 do i=1,ncount
   nfiles(i)=nproc+i-1
 enddo
 if (nproc_world > nproc) then
   call MPI_Group_excl(group_world,ncount,nfiles,group_worker,ierr)
   call MPI_Comm_create(comm_world,group_worker,group_id,ierr)
   call MPI_Group_free(group_worker,ierr)
   call MPI_Group_free(group_world,ierr)
 else
   group_id=comm_world
   myid=myid_world
 endif
 call MPI_Barrier(comm_world, ierr)
!------------------------------------------------------------------
! Inputing Earth model parameters 
!------------------------------------------------------------------
 if(myid_world >= nproc) return
 if(nproc_world>nproc) call MPI_Comm_rank(group_id,myid,ierr)
 call input_material(fileMod,group_id,myid,nproc_x,nproc_y, &
                     nd2_pml,RF,dt_fd,ierr)
! write(*,*) ' after call input_material'
 npt_fd=int(tdura/dt_fd+1.5)
 npt_out=(npt_fd-1)/intprt+1
 call MPI_BCAST (npt_out,1,MPI_INTEGER,0,group_id,ierr)
 dt_out = intprt*dt_fd
 npt_fd = (npt_out-1)*intprt+1
 call MPI_Barrier(group_id, ierr)
!------------------------------------------------------------------
! Inputing Receiver locations 
!------------------------------------------------------------------
 nfiles=0
 call input_Receiver_location(fileRec,group_id,myid,nr_all, &
               nrecs,xref_fdm,yref_fdm,angle_north_to_x,ierr)
! write(*,*) ' after call input_Receiver_location'
 if (ierr /= 0) then
   call MPI_FINALIZE(ierr)
   stop
 endif
 nfiles(myid+1) = nrecs
 call MPI_Barrier(group_id, ierr)
 num_fout=0
 do k=1,nproc
   call MPI_BCAST (nfiles(k),1,MPI_INTEGER,k-1,group_id,ierr)
   if(nfiles(k) > 0) num_fout=num_fout+1
 enddo
 if (ierr /= 0) call MPI_FINALIZE(ierr)
 ndata=num_src_model*nrecs*3
 if(nrecs > 0) then
   allocate(syn_dti(nrecs*3))
   !debug---------------------------------------
   !write (*,*) 'nrecs * 3 = ', nrecs * 3
   !write (*,*) syn_dti
   !--------------------------------------------
 endif
 if(myid == 0) close(13)
 call MPI_Barrier(group_id, ierr)
 return
end subroutine input_fd_param
!
!+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
subroutine run_fd_simul(myid_world)
 use mpi
 use comput_input_param
 use station_comm
 use grid_node_comm
 use wave_field_comm
 use source_parm_comm
 use itface_comm
 use, intrinsic :: iso_c_binding
 use metadata
 use logging
 use ctimer
! use c_call_interface
! include 'c_call_interface.inc'
!
 !------------------------
! function wdiff(tm, ierrtime)
! implicit none
! double precision::wdiff
! double precision, dimension(2)::tm
! integer::ierrtime
! end function wdiff
! end interface

 implicit NONE
! include 'c_call_interface.inc'
 interface
     subroutine set_deviceC(deviceID) bind(c, name='set_deviceC')
      use, intrinsic :: iso_c_binding, ONLY: C_INT
      integer(c_int), intent(in) :: deviceID
     end subroutine set_deviceC
     
     subroutine print_arrayC(arr, length, nameS) bind(c, name='print_arrayC')
       use, intrinsic :: iso_c_binding, ONLY: C_INT, C_CHAR, C_NULL_CHAR
       integer(c_int), dimension(*), intent(in) :: arr
       integer(c_int), intent(in) :: length
       character(kind=c_char), intent(in) :: nameS
     end subroutine print_arrayC

     subroutine allocate_gpu_memC(lbx, lby, nmat, mw1_pml1, mw2_pml1, nxtop, &
                            nytop, nztop, mw1_pml, mw2_pml, nxbtm, nybtm, &
                            nzbtm, nzbm1, nll) bind(c, name='allocate_gpu_memC')
       use, intrinsic :: iso_c_binding, ONLY: C_INT
       integer(c_int), dimension(*), intent(in) :: lbx
       integer(c_int), dimension(*), intent(in) :: lby
       integer(c_int), intent(in) :: nmat
       integer(c_int), intent(in) :: mw1_pml1
       integer(c_int), intent(in) :: mw2_pml1
       integer(c_int), intent(in) :: nxtop
       integer(c_int), intent(in) :: nytop
       integer(c_int), intent(in) :: nztop
       integer(c_int), intent(in) :: mw1_pml
       integer(c_int), intent(in) :: mw2_pml
       integer(c_int), intent(in) :: nxbtm
       integer(c_int), intent(in) :: nybtm
       integer(c_int), intent(in) :: nzbtm
       integer(c_int), intent(in) :: nzbm1
       integer(c_int), intent(in) :: nll
     end subroutine allocate_gpu_memC

     subroutine cpy_h2d_velocityInputsCOneTime(lbx, lby, nd1_vel, & 
                   cptr_rho, cptr_drvh1, cptr_drti1, cptr_damp1_x, cptr_damp1_y, cptr_idmat1, &
                   cptr_dxi1, cptr_dyi1, cptr_dzi1, cptr_dxh1, cptr_dyh1, cptr_dzh1, cptr_t1xx, &
                   cptr_t1xy, cptr_t1xz, cptr_t1yy, cptr_t1yz, cptr_t1zz, cptr_v1x_px, cptr_v1y_px, &
                   cptr_v1z_px, cptr_v1x_py, cptr_v1y_py, cptr_v1z_py, &
                   nd2_vel, & 
                   cptr_drvh2, cptr_drti2, cptr_idmat2, cptr_damp2_x, cptr_damp2_y, cptr_damp2_z, &
                   cptr_dxi2, cptr_dyi2, cptr_dzi2, cptr_dxh2, cptr_dyh2, cptr_dzh2, cptr_t2xx, &
                   cptr_t2xy, cptr_t2xz, cptr_t2yy, cptr_t2yz, cptr_t2zz, cptr_v2x_px, cptr_v2y_px, &
                   cptr_v2z_px, cptr_v2x_py, cptr_v2y_py, cptr_v2z_py, cptr_v2x_pz, cptr_v2y_pz, cptr_v2z_pz,c_cix, c_ciy, c_chx, c_chy,  &
                   nmat, mw1_pml1, mw2_pml1, nxtop, nytop, nztop, mw1_pml, mw2_pml,  &
                   nxbtm, nybtm, nzbtm, nzbm1) bind(c, name='cpy_h2d_velocityInputsCOneTime')
       use, intrinsic :: iso_c_binding, ONLY: C_INT, C_DOUBLE, C_PTR
       integer(c_int), dimension(*), intent(in) :: lbx
       integer(c_int), dimension(*), intent(in) :: lby
       integer(c_int), dimension(*), intent(in) :: nd1_vel
       type (c_ptr), value, intent(in) :: cptr_rho
       type (c_ptr), value, intent(in) :: cptr_drvh1
       type (c_ptr), value, intent(in) :: cptr_drti1
       type (c_ptr), value, intent(in) :: cptr_damp1_x
       type (c_ptr), value, intent(in) :: cptr_damp1_y
       type (c_ptr), value, intent(in) :: cptr_idmat1
       type (c_ptr), value, intent(in) :: cptr_dxi1
       type (c_ptr), value, intent(in) :: cptr_dyi1
       type (c_ptr), value, intent(in) :: cptr_dzi1
       type (c_ptr), value, intent(in) :: cptr_dxh1
       type (c_ptr), value, intent(in) :: cptr_dyh1
       type (c_ptr), value, intent(in) :: cptr_dzh1
       type (c_ptr), value, intent(in) :: cptr_t1xx
       type (c_ptr), value, intent(in) :: cptr_t1xy
       type (c_ptr), value, intent(in) :: cptr_t1xz
       type (c_ptr), value, intent(in) :: cptr_t1yy
       type (c_ptr), value, intent(in) :: cptr_t1yz
       type (c_ptr), value, intent(in) :: cptr_t1zz
       type (c_ptr), value, intent(in) :: cptr_v1x_px
       type (c_ptr), value, intent(in) :: cptr_v1y_px
       type (c_ptr), value, intent(in) :: cptr_v1z_px
       type (c_ptr), value, intent(in) :: cptr_v1x_py
       type (c_ptr), value, intent(in) :: cptr_v1y_py
       type (c_ptr), value, intent(in) :: cptr_v1z_py
       integer(c_int), dimension(*) :: nd2_vel
       type (c_ptr), value, intent(in) :: cptr_drvh2
       type (c_ptr), value, intent(in) :: cptr_drti2
       type (c_ptr), value, intent(in) :: cptr_idmat2
       type (c_ptr), value, intent(in) :: cptr_damp2_x
       type (c_ptr), value, intent(in) :: cptr_damp2_y
       type (c_ptr), value, intent(in) :: cptr_damp2_z
       type (c_ptr), value, intent(in) :: cptr_dxi2
       type (c_ptr), value, intent(in) :: cptr_dyi2
       type (c_ptr), value, intent(in) :: cptr_dzi2
       type (c_ptr), value, intent(in) :: cptr_dxh2
       type (c_ptr), value, intent(in) :: cptr_dyh2
       type (c_ptr), value, intent(in) :: cptr_dzh2
       type (c_ptr), value, intent(in) :: cptr_t2xx
       type (c_ptr), value, intent(in) :: cptr_t2xy
       type (c_ptr), value, intent(in) :: cptr_t2xz
       type (c_ptr), value, intent(in) :: cptr_t2yy
       type (c_ptr), value, intent(in) :: cptr_t2yz
       type (c_ptr), value, intent(in) :: cptr_t2zz
       type (c_ptr), value, intent(in) :: cptr_v2x_px
       type (c_ptr), value, intent(in) :: cptr_v2y_px
       type (c_ptr), value, intent(in) :: cptr_v2z_px
       type (c_ptr), value, intent(in) :: cptr_v2x_py
       type (c_ptr), value, intent(in) :: cptr_v2y_py
       type (c_ptr), value, intent(in) :: cptr_v2z_py
       type (c_ptr), value, intent(in) :: cptr_v2x_pz
       type (c_ptr), value, intent(in) :: cptr_v2y_pz
       type (c_ptr), value, intent(in) :: cptr_v2z_pz
       type(c_ptr), value, intent(in):: c_cix, c_ciy, c_chx, c_chy
       integer(c_int), intent(in) :: nmat
       integer(c_int), intent(in) :: mw1_pml1
       integer(c_int), intent(in) :: mw2_pml1
       integer(c_int), intent(in) :: nxtop
       integer(c_int), intent(in) :: nytop
       integer(c_int), intent(in) :: nztop
       integer(c_int), intent(in) :: mw1_pml
       integer(c_int), intent(in) :: mw2_pml
       integer(c_int), intent(in) :: nxbtm
       integer(c_int), intent(in) :: nybtm
       integer(c_int), intent(in) :: nzbtm
       integer(c_int), intent(in) :: nzbm1
     end subroutine cpy_h2d_velocityInputsCOneTime

     subroutine cpy_h2d_stressInputsCOneTime(lbx, lby, nd1_txy, nd1_txz, nd1_tyy, nd1_tyz, cptr_drti1, &
                    cptr_drth1,  cptr_damp1_x, cptr_damp1_y, cptr_idmat1, cptr_dxi1, cptr_dyi1, cptr_dzi1, & 
                    cptr_dxh1, cptr_dyh1, cptr_dzh1, cptr_v1x, cptr_v1y, cptr_v1z, &
                    cptr_t1xx_px, cptr_t1xy_px, cptr_t1xz_px, cptr_t1yy_px, cptr_qt1xx_px, cptr_qt1xy_px, &
                    cptr_qt1xz_px, cptr_qt1yy_px, cptr_t1xx_py, cptr_t1xy_py, cptr_t1yy_py, cptr_t1yz_py, &
                    cptr_qt1xx_py, cptr_qt1xy_py, cptr_qt1yy_py, cptr_qt1yz_py, cptr_qt1xx, cptr_qt1xy, &
                    cptr_qt1xz, cptr_qt1yy, cptr_qt1yz, cptr_qt1zz, cptr_clamda, cptr_cmu,  &
                    cptr_epdt, cptr_qwp, cptr_qws, cptr_qwt1, cptr_qwt2, nd2_txy, nd2_txz, nd2_tyy, &
                    nd2_tyz, cptr_drti2, cptr_drth2, cptr_idmat2, cptr_damp2_x, cptr_damp2_y, cptr_damp2_z, &
                    cptr_dxi2, cptr_dyi2, cptr_dzi2, cptr_dxh2, cptr_dyh2, cptr_dzh2, cptr_v2x, cptr_v2y, cptr_v2z, &
                    cptr_qt2xx, cptr_qt2xy, cptr_qt2xz, cptr_qt2yy, cptr_qt2yz, cptr_qt2zz, cptr_t2xx_px, cptr_t2xy_px,&
                    cptr_t2xz_px, cptr_t2yy_px, cptr_qt2xx_px, cptr_qt2xy_px, cptr_qt2xz_px, cptr_qt2yy_px, &
                    cptr_t2xx_py, cptr_t2xy_py, cptr_t2yy_py, cptr_t2yz_py, cptr_qt2xx_py, cptr_qt2xy_py, cptr_qt2yy_py,&
                    cptr_qt2yz_py, cptr_t2xx_pz, cptr_t2xz_pz, cptr_t2yz_pz, cptr_t2zz_pz, cptr_qt2xx_pz, cptr_qt2xz_pz,&
                    cptr_qt2yz_pz, cptr_qt2zz_pz, &
                    nmat, mw1_pml1,mw2_pml1, nxtop, nytop, nztop, mw1_pml,  mw2_pml, &
                    nxbtm, nybtm, nzbtm, nll) bind(c, name='cpy_h2d_stressInputsCOneTime')

       use, intrinsic :: iso_c_binding, ONLY: C_INT, C_DOUBLE, C_PTR
       integer(c_int), dimension(*), intent(in) :: lbx
       integer(c_int), dimension(*), intent(in) :: lby
       integer(c_int), dimension(*), intent(in) :: nd1_txy
       integer(c_int), dimension(*), intent(in) :: nd1_txz
       integer(c_int), dimension(*), intent(in) :: nd1_tyy
       integer(c_int), dimension(*), intent(in) :: nd1_tyz
       type (c_ptr), value, intent(in) :: cptr_drti1
       type (c_ptr), value, intent(in) :: cptr_drth1
       type (c_ptr), value, intent(in) :: cptr_damp1_x
       type (c_ptr), value, intent(in) :: cptr_damp1_y
       type (c_ptr), value, intent(in) :: cptr_idmat1
       type (c_ptr), value, intent(in) :: cptr_dxi1
       type (c_ptr), value, intent(in) :: cptr_dyi1
       type (c_ptr), value, intent(in) :: cptr_dzi1
       type (c_ptr), value, intent(in) :: cptr_dxh1
       type (c_ptr), value, intent(in) :: cptr_dyh1
       type (c_ptr), value, intent(in) :: cptr_dzh1
       type (c_ptr), value, intent(in) :: cptr_v1x
       type (c_ptr), value, intent(in) :: cptr_v1y
       type (c_ptr), value, intent(in) :: cptr_v1z
       type (c_ptr), value, intent(in) :: cptr_t1xx_px
       type (c_ptr), value, intent(in) :: cptr_t1xy_px
       type (c_ptr), value, intent(in) :: cptr_t1xz_px
       type (c_ptr), value, intent(in) :: cptr_t1yy_px
       type (c_ptr), value, intent(in) :: cptr_qt1xx_px
       type (c_ptr), value, intent(in) :: cptr_qt1xy_px
       type (c_ptr), value, intent(in) :: cptr_qt1xz_px
       type (c_ptr), value, intent(in) :: cptr_qt1yy_px
       type (c_ptr), value, intent(in) :: cptr_t1xx_py
       type (c_ptr), value, intent(in) :: cptr_t1xy_py
       type (c_ptr), value, intent(in) :: cptr_t1yy_py
       type (c_ptr), value, intent(in) :: cptr_t1yz_py
       type (c_ptr), value, intent(in) :: cptr_qt1xx_py
       type (c_ptr), value, intent(in) :: cptr_qt1xy_py
       type (c_ptr), value, intent(in) :: cptr_qt1yy_py
       type (c_ptr), value, intent(in) :: cptr_qt1yz_py
       type (c_ptr), value, intent(in) :: cptr_qt1xx
       type (c_ptr), value, intent(in) :: cptr_qt1xy
       type (c_ptr), value, intent(in) :: cptr_qt1xz
       type (c_ptr), value, intent(in) :: cptr_qt1yy
       type (c_ptr), value, intent(in) :: cptr_qt1yz
       type (c_ptr), value, intent(in) :: cptr_qt1zz
       type (c_ptr), value, intent(in) :: cptr_clamda
       type (c_ptr), value, intent(in) :: cptr_cmu
       type (c_ptr), value, intent(in) :: cptr_epdt
       type (c_ptr), value, intent(in) :: cptr_qwp
       type (c_ptr), value, intent(in) :: cptr_qws
       type (c_ptr), value, intent(in) :: cptr_qwt1
       type (c_ptr), value, intent(in) :: cptr_qwt2
       integer(c_int), dimension(*) :: nd2_txy
       integer(c_int), dimension(*) :: nd2_txz
       integer(c_int), dimension(*) :: nd2_tyy
       integer(c_int), dimension(*) :: nd2_tyz
       type (c_ptr), value, intent(in) :: cptr_drti2
       type (c_ptr), value, intent(in) :: cptr_drth2
       type (c_ptr), value, intent(in) :: cptr_idmat2
       type (c_ptr), value, intent(in) :: cptr_damp2_x
       type (c_ptr), value, intent(in) :: cptr_damp2_y
       type (c_ptr), value, intent(in) :: cptr_damp2_z
       type (c_ptr), value, intent(in) :: cptr_dxi2
       type (c_ptr), value, intent(in) :: cptr_dyi2
       type (c_ptr), value, intent(in) :: cptr_dzi2
       type (c_ptr), value, intent(in) :: cptr_dxh2
       type (c_ptr), value, intent(in) :: cptr_dyh2
       type (c_ptr), value, intent(in) :: cptr_dzh2
       type (c_ptr), value, intent(in) :: cptr_v2x
       type (c_ptr), value, intent(in) :: cptr_v2y
       type (c_ptr), value, intent(in) :: cptr_v2z
       type (c_ptr), value, intent(in) :: cptr_qt2xx
       type (c_ptr), value, intent(in) :: cptr_qt2xy
       type (c_ptr), value, intent(in) :: cptr_qt2xz
       type (c_ptr), value, intent(in) :: cptr_qt2yy
       type (c_ptr), value, intent(in) :: cptr_qt2yz
       type (c_ptr), value, intent(in) :: cptr_qt2zz
       type (c_ptr), value, intent(in) :: cptr_t2xx_px
       type (c_ptr), value, intent(in) :: cptr_t2xy_px
       type (c_ptr), value, intent(in) :: cptr_t2xz_px
       type (c_ptr), value, intent(in) :: cptr_t2yy_px
       type (c_ptr), value, intent(in) :: cptr_qt2xx_px
       type (c_ptr), value, intent(in) :: cptr_qt2xy_px
       type (c_ptr), value, intent(in) :: cptr_qt2xz_px
       type (c_ptr), value, intent(in) :: cptr_qt2yy_px
       type (c_ptr), value, intent(in) :: cptr_t2xx_py
       type (c_ptr), value, intent(in) :: cptr_t2xy_py
       type (c_ptr), value, intent(in) :: cptr_t2yy_py
       type (c_ptr), value, intent(in) :: cptr_t2yz_py
       type (c_ptr), value, intent(in) :: cptr_qt2xx_py
       type (c_ptr), value, intent(in) :: cptr_qt2xy_py
       type (c_ptr), value, intent(in) :: cptr_qt2yy_py
       type (c_ptr), value, intent(in) :: cptr_qt2yz_py
       type (c_ptr), value, intent(in) :: cptr_t2xx_pz
       type (c_ptr), value, intent(in) :: cptr_t2xz_pz
       type (c_ptr), value, intent(in) :: cptr_t2yz_pz
       type (c_ptr), value, intent(in) :: cptr_t2zz_pz
       type (c_ptr), value, intent(in) :: cptr_qt2xx_pz
       type (c_ptr), value, intent(in) :: cptr_qt2xz_pz
       type (c_ptr), value, intent(in) :: cptr_qt2yz_pz
       type (c_ptr), value, intent(in) :: cptr_qt2zz_pz
       integer(c_int), intent(in) :: nmat
       integer(c_int), intent(in) :: mw1_pml1
       integer(c_int), intent(in) :: mw2_pml1
       integer(c_int), intent(in) :: nxtop
       integer(c_int), intent(in) :: nytop
       integer(c_int), intent(in) :: nztop
       integer(c_int), intent(in) :: mw1_pml
       integer(c_int), intent(in) :: mw2_pml
       integer(c_int), intent(in) :: nxbtm
       integer(c_int), intent(in) :: nybtm
       integer(c_int), intent(in) :: nzbtm
       integer(c_int), intent(in) :: nll
     end subroutine cpy_h2d_stressInputsCOneTime


!     subroutine cpy_h2d_velocityOutputsC(lbx, lby, &
!                   cptr_v1x, cptr_v1y, cptr_v1z, cptr_v1x_px, cptr_v1y_px, cptr_v1z_px, &
!                   cptr_v1x_py, cptr_v1y_py, cptr_v1z_py, &
!                   cptr_v2x, cptr_v2y, cptr_v2z, cptr_v2x_px, cptr_v2y_px, cptr_v2z_px, &
!                   cptr_v2x_py, cptr_v2y_py, cptr_v2z_py, cptr_v2x_pz, cptr_v2y_pz, cptr_v2z_pz, &
!                   mw1_pml1, mw2_pml1, nxtop, nytop, nztop, mw1_pml, &
!                   mw2_pml, nxbtm, nybtm, nzbtm) bind(c, name='data_cpy_h2d_outputsC')
!       use, intrinsic :: iso_c_binding, ONLY: C_INT, C_DOUBLE, C_PTR
!       integer(c_int), dimension(*), intent(in) :: lbx
!       integer(c_int), dimension(*), intent(in) :: lby
!       type (c_ptr), intent(inout) :: cptr_v1x
!       type (c_ptr), intent(inout) :: cptr_v1y
!       type (c_ptr), intent(inout) :: cptr_v1z
!       type (c_ptr), value, intent(in) :: cptr_v1x_px
!       type (c_ptr), value, intent(in) :: cptr_v1y_px
!       type (c_ptr), value, intent(in) :: cptr_v1z_px
!       type (c_ptr), value, intent(in) :: cptr_v1x_py
!       type (c_ptr), value, intent(in) :: cptr_v1y_py
!       type (c_ptr), value, intent(in) :: cptr_v1z_py
!       type (c_ptr), intent(inout) :: cptr_v2x
!       type (c_ptr), intent(inout) :: cptr_v2y
!       type (c_ptr), intent(inout) :: cptr_v2z
!       type (c_ptr), value, intent(in) :: cptr_v2x_px
!       type (c_ptr), value, intent(in) :: cptr_v2y_px
!       type (c_ptr), value, intent(in) :: cptr_v2z_px
!       type (c_ptr), value, intent(in) :: cptr_v2x_py
!       type (c_ptr), value, intent(in) :: cptr_v2y_py
!       type (c_ptr), value, intent(in) :: cptr_v2z_py
!       type (c_ptr), value, intent(in) :: cptr_v2x_pz
!       type (c_ptr), value, intent(in) :: cptr_v2y_pz
!       type (c_ptr), value, intent(in) :: cptr_v2z_pz
!       integer(c_int), intent(in) :: mw1_pml1
!       integer(c_int), intent(in) :: mw2_pml1
!       integer(c_int), intent(in) :: nxtop
!       integer(c_int), intent(in) :: nytop
!       integer(c_int), intent(in) :: nztop
!       integer(c_int), intent(in) :: mw1_pml
!       integer(c_int), intent(in) :: mw2_pml
!       integer(c_int), intent(in) :: nxbtm
!       integer(c_int), intent(in) :: nybtm
!       integer(c_int), intent(in) :: nzbtm
!     end subroutine data_cpy_h2d_outputsC

     subroutine cpy_d2h_velocityOutputsC ( cptr_v1x, cptr_v1y, cptr_v1z, &
                   cptr_v2x, cptr_v2y, cptr_v2z, &
                   nxtop, nytop, nztop, &
                   nxbtm, nybtm, nzbtm) bind(c, name='cpy_d2h_velocityOutputsC')
       use, intrinsic :: iso_c_binding, ONLY: C_INT, C_DOUBLE, C_PTR
       type (c_ptr), intent(in), value :: cptr_v1x
       type (c_ptr), intent(in), value :: cptr_v1y
       type (c_ptr), intent(in), value :: cptr_v1z
       type (c_ptr), intent(in), value :: cptr_v2x
       type (c_ptr), intent(in), value :: cptr_v2y
       type (c_ptr), intent(in), value :: cptr_v2z
       integer(c_int), intent(in) :: nxtop
       integer(c_int), intent(in) :: nytop
       integer(c_int), intent(in) :: nztop
       integer(c_int), intent(in) :: nxbtm
       integer(c_int), intent(in) :: nybtm
       integer(c_int), intent(in) :: nzbtm
     end subroutine cpy_d2h_velocityOutputsC

     subroutine compute_velocityCDummy (nxtop, nztop, nytop, nxbtm, nzbtm, &
     nybtm, cptr_v1x, cptr_v2x,  cptr_v1y, cptr_v2y,    cptr_v1z,  cptr_v2z) &
     bind(C, name="compute_velocityCDummy")
        use, intrinsic :: iso_c_binding, ONLY: C_INT, C_DOUBLE, C_PTR
        integer(c_int), intent(in) :: nxtop, nztop, nytop, nxbtm, nzbtm, nybtm
        type(c_ptr), intent(inout) :: cptr_v1x, cptr_v2x, cptr_v1y, cptr_v2y, cptr_v1z, cptr_v2z
     end subroutine compute_velocityCDummy

     subroutine compute_velocityC(nztop, nztm1, ca, lbx, lby, nd1_vel, cptr_rho, &
                            cptr_drvh1, cptr_drti1, cptr_damp1_x, cptr_damp1_y, cptr_idmat1, &
                            cptr_dxi1, cptr_dyi1, cptr_dzi1, cptr_dxh1, cptr_dyh1, cptr_dzh1, &
                            cptr_t1xx, cptr_t1xy, cptr_t1xz, cptr_t1yy, cptr_t1yz, cptr_t1zz, &
                            cptr_v1x, cptr_v1y, cptr_v1z, cptr_v1x_px, cptr_v1y_px, cptr_v1z_px, &
                            cptr_v1x_py, cptr_v1y_py, cptr_v1z_py, nzbm1, & 
                            nd2_vel, cptr_drvh2, cptr_drti2, cptr_idmat2, cptr_damp2_x, &
                            cptr_damp2_y, cptr_damp2_z, cptr_dxi2, cptr_dyi2, cptr_dzi2, &
                            cptr_dxh2, cptr_dyh2, cptr_dzh2, cptr_t2xx, cptr_t2xy, cptr_t2xz, &
                            cptr_t2yy, cptr_t2yz, cptr_t2zz, cptr_v2x, cptr_v2y, cptr_v2z,&
                            cptr_v2x_px, cptr_v2y_px,cptr_v2z_px, cptr_v2x_py, cptr_v2y_py, &
                            cptr_v2z_py, cptr_v2x_pz, cptr_v2y_pz, cptr_v2z_pz,&
                            nmat, mw1_pml1, mw2_pml1, nxtop, nytop, mw1_pml, mw2_pml, nxbtm, &
                            nybtm, nzbtm, myid) bind(c, name='compute_velocityC') 
       use mpi
       use, intrinsic :: iso_c_binding, ONLY: C_INT, C_DOUBLE, C_PTR
       integer(c_int), intent(in) :: nztop
       integer(c_int), intent(in) :: nztm1
       real(kind=c_double), intent(in) :: ca
       integer(c_int), dimension(*), intent(in) :: lbx
       integer(c_int), dimension(*), intent(in) :: lby
       integer(c_int), dimension(*), intent(in) :: nd1_vel
       type (c_ptr), value, intent(in) :: cptr_rho
       type (c_ptr), value, intent(in) :: cptr_drvh1
       type (c_ptr), value, intent(in) :: cptr_drti1
       type (c_ptr), value, intent(in) :: cptr_damp1_x
       type (c_ptr), value, intent(in) :: cptr_damp1_y
       type (c_ptr), value, intent(in) :: cptr_idmat1
       type (c_ptr), value, intent(in) :: cptr_dxi1
       type (c_ptr), value, intent(in) :: cptr_dyi1
       type (c_ptr), value, intent(in) :: cptr_dzi1
       type (c_ptr), value, intent(in) :: cptr_dxh1
       type (c_ptr), value, intent(in) :: cptr_dyh1
       type (c_ptr), value, intent(in) :: cptr_dzh1
       type (c_ptr), value, intent(in) :: cptr_t1xx
       type (c_ptr), value, intent(in) :: cptr_t1xy
       type (c_ptr), value, intent(in) :: cptr_t1xz
       type (c_ptr), value, intent(in) :: cptr_t1yy
       type (c_ptr), value, intent(in) :: cptr_t1yz
       type (c_ptr), value, intent(in) :: cptr_t1zz
       type (c_ptr), intent(inout) :: cptr_v1x
       type (c_ptr), intent(inout) :: cptr_v1y
       type (c_ptr), intent(inout) :: cptr_v1z
       type (c_ptr), value, intent(in) :: cptr_v1x_px
       type (c_ptr), value, intent(in) :: cptr_v1y_px
       type (c_ptr), value, intent(in) :: cptr_v1z_px
       type (c_ptr), value, intent(in) :: cptr_v1x_py
       type (c_ptr), value, intent(in) :: cptr_v1y_py
       type (c_ptr), value, intent(in) :: cptr_v1z_py
       integer(c_int), intent(in) :: nzbm1
       integer(c_int), dimension(*) :: nd2_vel
       type (c_ptr), value, intent(in) :: cptr_drvh2
       type (c_ptr), value, intent(in) :: cptr_drti2
       type (c_ptr), value, intent(in) :: cptr_idmat2
       type (c_ptr), value, intent(in) :: cptr_damp2_x
       type (c_ptr), value, intent(in) :: cptr_damp2_y
       type (c_ptr), value, intent(in) :: cptr_damp2_z
       type (c_ptr), value, intent(in) :: cptr_dxi2
       type (c_ptr), value, intent(in) :: cptr_dyi2
       type (c_ptr), value, intent(in) :: cptr_dzi2
       type (c_ptr), value, intent(in) :: cptr_dxh2
       type (c_ptr), value, intent(in) :: cptr_dyh2
       type (c_ptr), value, intent(in) :: cptr_dzh2
       type (c_ptr), value, intent(in) :: cptr_t2xx
       type (c_ptr), value, intent(in) :: cptr_t2xy
       type (c_ptr), value, intent(in) :: cptr_t2xz
       type (c_ptr), value, intent(in) :: cptr_t2yy
       type (c_ptr), value, intent(in) :: cptr_t2yz
       type (c_ptr), value, intent(in) :: cptr_t2zz
       type (c_ptr), intent(inout) :: cptr_v2x
       type (c_ptr), intent(inout) :: cptr_v2y
       type (c_ptr), intent(inout) :: cptr_v2z
       type (c_ptr), value, intent(in) :: cptr_v2x_px
       type (c_ptr), value, intent(in) :: cptr_v2y_px
       type (c_ptr), value, intent(in) :: cptr_v2z_px
       type (c_ptr), value, intent(in) :: cptr_v2x_py
       type (c_ptr), value, intent(in) :: cptr_v2y_py
       type (c_ptr), value, intent(in) :: cptr_v2z_py
       type (c_ptr), value, intent(in) :: cptr_v2x_pz
       type (c_ptr), value, intent(in) :: cptr_v2y_pz
       type (c_ptr), value, intent(in) :: cptr_v2z_pz
       integer(c_int), intent(in) :: nmat
       integer(c_int), intent(in) :: mw1_pml1
       integer(c_int), intent(in) :: mw2_pml1
       integer(c_int), intent(in) :: nxtop
       integer(c_int), intent(in) :: nytop
       integer(c_int), intent(in) :: mw1_pml
       integer(c_int), intent(in) :: mw2_pml
       integer(c_int), intent(in) :: nxbtm
       integer(c_int), intent(in) :: nybtm
       integer(c_int), intent(in) :: nzbtm
       integer(c_int), intent(in) :: myid
     end subroutine compute_velocityC

     subroutine compute_stressC(nxb1, nyb1, nx1p1, ny1p1, nxtop, nytop, nztop, mw1_pml, mw1_pml1, &
                   nmat, nll, lbx, lby, nd1_txy, nd1_txz, nd1_tyy, nd1_tyz, cptr_idmat1, ca, cptr_drti1, &
                   cptr_drth1, cptr_damp1_x, cptr_damp1_y, cptr_clamda, cptr_cmu, cptr_epdt, &
                   cptr_qwp, cptr_qws, cptr_qwt1, cptr_qwt2, cptr_dxh1, cptr_dyh1, cptr_dzh1, &
                   cptr_dxi1, cptr_dyi1, cptr_dzi1, cptr_t1xx, cptr_t1xy, cptr_t1xz, cptr_t1yy, &
                   cptr_t1yz, cptr_t1zz, cptr_qt1xx, cptr_qt1xy, cptr_qt1xz, cptr_qt1yy, cptr_qt1yz, &
                   cptr_qt1zz, cptr_t1xx_px, cptr_t1xy_px, cptr_t1xz_px, cptr_t1yy_px, cptr_qt1xx_px, &
                   cptr_qt1xy_px, cptr_qt1xz_px, cptr_qt1yy_px, cptr_t1xx_py, cptr_t1xy_py, cptr_t1yy_py, &
                   cptr_t1yz_py, cptr_qt1xx_py, cptr_qt1xy_py, cptr_qt1yy_py, cptr_qt1yz_py, cptr_v1x, &
                   cptr_v1y, cptr_v1z,&
                   nxb2, nyb2, nxbtm, nybtm, nzbtm, mw2_pml, mw2_pml1, nd2_txy, nd2_txz, nd2_tyy, nd2_tyz, &
                   cptr_idmat2, cptr_drti2, cptr_drth2, cptr_damp2_x, cptr_damp2_y, cptr_damp2_z, &
                   cptr_t2xx, cptr_t2xy, cptr_t2xz, cptr_t2yy, cptr_t2yz, cptr_t2zz, cptr_qt2xx, cptr_qt2xy, cptr_qt2xz, cptr_qt2yy, &
                   cptr_qt2yz, cptr_qt2zz, cptr_dxh2, cptr_dyh2, cptr_dzh2, cptr_dxi2, cptr_dyi2, cptr_dzi2, cptr_t2xx_px, &
                   cptr_t2xy_px, cptr_t2xz_px, cptr_t2yy_px, cptr_t2xx_py, cptr_t2xy_py, cptr_t2yy_py, cptr_t2yz_py, &
                   cptr_t2xx_pz, cptr_t2xz_pz, cptr_t2yz_pz, cptr_t2zz_pz, cptr_qt2xx_px, cptr_qt2xy_px, cptr_qt2xz_px, &
                   cptr_qt2yy_px, cptr_qt2xx_py, cptr_qt2xy_py, cptr_qt2yy_py, cptr_qt2yz_py, cptr_qt2xx_pz, cptr_qt2xz_pz, &
                   cptr_qt2yz_pz, cptr_qt2zz_pz, cptr_v2x, cptr_v2y, cptr_v2z, myid) bind(c, name='compute_stressC')
       use mpi
       use, intrinsic :: iso_c_binding, ONLY: C_INT, C_DOUBLE, C_PTR
       integer(c_int), intent(in) :: nxb1
       integer(c_int), intent(in) :: nyb1
       integer(c_int), intent(in) :: nx1p1
       integer(c_int), intent(in) :: ny1p1
       integer(c_int), intent(in) :: nxtop
       integer(c_int), intent(in) :: nytop
       integer(c_int), intent(in) :: nztop
       integer(c_int), intent(in) :: mw1_pml
       integer(c_int), intent(in) :: mw1_pml1
       integer(c_int), intent(in) :: nmat
       integer(c_int), intent(in) :: nll
       integer(c_int), dimension(*), intent(in) :: lbx
       integer(c_int), dimension(*), intent(in) :: lby
       integer(c_int), dimension(*), intent(in) :: nd1_txy
       integer(c_int), dimension(*), intent(in) :: nd1_txz
       integer(c_int), dimension(*), intent(in) :: nd1_tyy
       integer(c_int), dimension(*), intent(in) :: nd1_tyz
       type (c_ptr), value, intent(in) :: cptr_idmat1
       real(kind=c_double), intent(in) :: ca
       type (c_ptr), value, intent(in) :: cptr_drti1
       type (c_ptr), value, intent(in) :: cptr_drth1
       type (c_ptr), value, intent(in) :: cptr_damp1_x
       type (c_ptr), value, intent(in) :: cptr_damp1_y
       type (c_ptr), value, intent(in) :: cptr_clamda
       type (c_ptr), value, intent(in) :: cptr_cmu
       type (c_ptr), value, intent(in) :: cptr_epdt
       type (c_ptr), value, intent(in) :: cptr_qwp
       type (c_ptr), value, intent(in) :: cptr_qws
       type (c_ptr), value, intent(in) :: cptr_qwt1
       type (c_ptr), value, intent(in) :: cptr_qwt2
       type (c_ptr), value, intent(in) :: cptr_dxh1
       type (c_ptr), value, intent(in) :: cptr_dyh1
       type (c_ptr), value, intent(in) :: cptr_dzh1
       type (c_ptr), value, intent(in) :: cptr_dxi1
       type (c_ptr), value, intent(in) :: cptr_dyi1
       type (c_ptr), value, intent(in) :: cptr_dzi1
       type (c_ptr), value, intent(in) :: cptr_t1xx
       type (c_ptr), value, intent(in) :: cptr_t1xy
       type (c_ptr), value, intent(in) :: cptr_t1xz
       type (c_ptr), value, intent(in) :: cptr_t1yy
       type (c_ptr), value, intent(in) :: cptr_t1yz
       type (c_ptr), value, intent(in) :: cptr_t1zz
       type (c_ptr), value, intent(in) :: cptr_qt1xx
       type (c_ptr), value, intent(in) :: cptr_qt1xy
       type (c_ptr), value, intent(in) :: cptr_qt1xz
       type (c_ptr), value, intent(in) :: cptr_qt1yy
       type (c_ptr), value, intent(in) :: cptr_qt1yz
       type (c_ptr), value, intent(in) :: cptr_qt1zz
       type (c_ptr), value, intent(in) :: cptr_t1xx_px
       type (c_ptr), value, intent(in) :: cptr_t1xy_px
       type (c_ptr), value, intent(in) :: cptr_t1xz_px
       type (c_ptr), value, intent(in) :: cptr_t1yy_px
       type (c_ptr), value, intent(in) :: cptr_qt1xx_px
       type (c_ptr), value, intent(in) :: cptr_qt1xy_px
       type (c_ptr), value, intent(in) :: cptr_qt1xz_px
       type (c_ptr), value, intent(in) :: cptr_qt1yy_px
       type (c_ptr), value, intent(in) :: cptr_t1xx_py
       type (c_ptr), value, intent(in) :: cptr_t1xy_py
       type (c_ptr), value, intent(in) :: cptr_t1yy_py
       type (c_ptr), value, intent(in) :: cptr_t1yz_py
       type (c_ptr), value, intent(in) :: cptr_qt1xx_py
       type (c_ptr), value, intent(in) :: cptr_qt1xy_py
       type (c_ptr), value, intent(in) :: cptr_qt1yy_py
       type (c_ptr), value, intent(in) :: cptr_qt1yz_py
       type (c_ptr), intent(inout) :: cptr_v1x
       type (c_ptr), intent(inout) :: cptr_v1y
       type (c_ptr), intent(inout) :: cptr_v1z
       integer(c_int), intent(in) :: nxb2
       integer(c_int), intent(in) :: nyb2
       integer(c_int), intent(in) :: nxbtm
       integer(c_int), intent(in) :: nybtm
       integer(c_int), intent(in) :: nzbtm
       integer(c_int), intent(in) :: mw2_pml
       integer(c_int), intent(in) :: mw2_pml1
       integer(c_int), dimension(*), intent(in) :: nd2_txy
       integer(c_int), dimension(*), intent(in) :: nd2_txz
       integer(c_int), dimension(*), intent(in) :: nd2_tyy
       integer(c_int), dimension(*), intent(in) :: nd2_tyz
       type (c_ptr), value, intent(in) :: cptr_idmat2
       type (c_ptr), value, intent(in) :: cptr_drti2
       type (c_ptr), value, intent(in) :: cptr_drth2
       type (c_ptr), value, intent(in) :: cptr_damp2_x
       type (c_ptr), value, intent(in) :: cptr_damp2_y
       type (c_ptr), value, intent(in) :: cptr_damp2_z
       type (c_ptr), value, intent(in) :: cptr_t2xx
       type (c_ptr), value, intent(in) :: cptr_t2xy
       type (c_ptr), value, intent(in) :: cptr_t2xz
       type (c_ptr), value, intent(in) :: cptr_t2yy
       type (c_ptr), value, intent(in) :: cptr_t2yz
       type (c_ptr), value, intent(in) :: cptr_t2zz
       type (c_ptr), value, intent(in) :: cptr_qt2xx
       type (c_ptr), value, intent(in) :: cptr_qt2xy
       type (c_ptr), value, intent(in) :: cptr_qt2xz
       type (c_ptr), value, intent(in) :: cptr_qt2yy
       type (c_ptr), value, intent(in) :: cptr_qt2yz
       type (c_ptr), value, intent(in) :: cptr_qt2zz
       type (c_ptr), value, intent(in) :: cptr_dxh2
       type (c_ptr), value, intent(in) :: cptr_dyh2
       type (c_ptr), value, intent(in) :: cptr_dzh2
       type (c_ptr), value, intent(in) :: cptr_dxi2
       type (c_ptr), value, intent(in) :: cptr_dyi2
       type (c_ptr), value, intent(in) :: cptr_dzi2
       type (c_ptr), value, intent(in) :: cptr_t2xx_px
       type (c_ptr), value, intent(in) :: cptr_t2xy_px
       type (c_ptr), value, intent(in) :: cptr_t2xz_px
       type (c_ptr), value, intent(in) :: cptr_t2yy_px
       type (c_ptr), value, intent(in) :: cptr_t2xx_py
       type (c_ptr), value, intent(in) :: cptr_t2xy_py
       type (c_ptr), value, intent(in) :: cptr_t2yy_py
       type (c_ptr), value, intent(in) :: cptr_t2yz_py
       type (c_ptr), value, intent(in) :: cptr_t2xx_pz
       type (c_ptr), value, intent(in) :: cptr_t2xz_pz
       type (c_ptr), value, intent(in) :: cptr_t2yz_pz
       type (c_ptr), value, intent(in) :: cptr_t2zz_pz
       type (c_ptr), value, intent(in) :: cptr_qt2xx_px
       type (c_ptr), value, intent(in) :: cptr_qt2xy_px
       type (c_ptr), value, intent(in) :: cptr_qt2xz_px
       type (c_ptr), value, intent(in) :: cptr_qt2yy_px
       type (c_ptr), value, intent(in) :: cptr_qt2xx_py
       type (c_ptr), value, intent(in) :: cptr_qt2xy_py
       type (c_ptr), value, intent(in) :: cptr_qt2yy_py
       type (c_ptr), value, intent(in) :: cptr_qt2yz_py
       type (c_ptr), value, intent(in) :: cptr_qt2xx_pz
       type (c_ptr), value, intent(in) :: cptr_qt2xz_pz
       type (c_ptr), value, intent(in) :: cptr_qt2yz_pz
       type (c_ptr), value, intent(in) :: cptr_qt2zz_pz
       type (c_ptr), intent(inout) :: cptr_v2x
       type (c_ptr), intent(inout) :: cptr_v2y
       type (c_ptr), intent(inout) :: cptr_v2z
       integer(c_int), intent(in) :: myid
    end subroutine compute_stressC

    subroutine compute_stressCDebug(nxb1, nyb1, nx1p1, ny1p1, nxtop, nytop, nztop, mw1_pml, mw1_pml1, &
                   lbx, lby, nd1_txy, nd1_txz, nd1_tyy, nd1_tyz, cptr_idmat1, ca, cptr_drti1, &
                   cptr_drth1, cptr_damp1_x, cptr_damp1_y, cptr_clamda, cptr_cmu, cptr_epdt, &
                   cptr_qwp, cptr_qws, cptr_qwt1, cptr_qwt2, cptr_dxh1, cptr_dyh1, cptr_dzh1, &
                   cptr_dxi1, cptr_dyi1, cptr_dzi1, cptr_t1xx, cptr_t1xy, cptr_t1xz, cptr_t1yy, &
                   cptr_t1yz, cptr_t1zz, cptr_qt1xx, cptr_qt1xy, cptr_qt1xz, cptr_qt1yy, cptr_qt1yz, &
                   cptr_qt1zz, cptr_t1xx_px, cptr_t1xy_px, cptr_t1xz_px, cptr_t1yy_px, cptr_qt1xx_px, &
                   cptr_qt1xy_px, cptr_qt1xz_px, cptr_qt1yy_px, cptr_t1xx_py, cptr_t1xy_py, cptr_t1yy_py, &
                   cptr_t1yz_py, cptr_qt1xx_py, cptr_qt1xy_py, cptr_qt1yy_py, cptr_qt1yz_py, cptr_v1x, &
                   cptr_v1y, cptr_v1z,&
                   nxb2, nyb2, nxbtm, nybtm, nzbtm, mw2_pml, mw2_pml1, nd2_txy, nd2_txz, nd2_tyy, nd2_tyz, &
                   cptr_idmat2, cptr_drti2, cptr_drth2, cptr_damp2_x, cptr_damp2_y, cptr_damp2_z, &
                   cptr_t2xx, cptr_t2xy, cptr_t2xz, cptr_t2yy, cptr_t2yz, cptr_t2zz, cptr_qt2xx, cptr_qt2xy, cptr_qt2xz, cptr_qt2yy, &
                   cptr_qt2yz, cptr_qt2zz, cptr_dxh2, cptr_dyh2, cptr_dzh2, cptr_dxi2, cptr_dyi2, cptr_dzi2, cptr_t2xx_px, &
                   cptr_t2xy_px, cptr_t2xz_px, cptr_t2yy_px, cptr_t2xx_py, cptr_t2xy_py, cptr_t2yy_py, cptr_t2yz_py, &
                   cptr_t2xx_pz, cptr_t2xz_pz, cptr_t2yz_pz, cptr_t2zz_pz, cptr_qt2xx_px, cptr_qt2xy_px, cptr_qt2xz_px, &
                   cptr_qt2yy_px, cptr_qt2xx_py, cptr_qt2xy_py, cptr_qt2yy_py, cptr_qt2yz_py, cptr_qt2xx_pz, cptr_qt2xz_pz, &
                   cptr_qt2yz_pz, cptr_qt2zz_pz, cptr_v2x, cptr_v2y, cptr_v2z, myid) bind(c, name='compute_stressCDebug')
       use mpi
       use, intrinsic :: iso_c_binding, ONLY: C_INT, C_DOUBLE, C_PTR
       integer(c_int), intent(in) :: nxb1
       integer(c_int), intent(in) :: nyb1
       integer(c_int), intent(in) :: nx1p1
       integer(c_int), intent(in) :: ny1p1
       integer(c_int), intent(in) :: nxtop
       integer(c_int), intent(in) :: nytop
       integer(c_int), intent(in) :: nztop
       integer(c_int), intent(in) :: mw1_pml
       integer(c_int), intent(in) :: mw1_pml1
       integer(c_int), dimension(*), intent(in) :: lbx
       integer(c_int), dimension(*), intent(in) :: lby
       integer(c_int), dimension(*), intent(in) :: nd1_txy
       integer(c_int), dimension(*), intent(in) :: nd1_txz
       integer(c_int), dimension(*), intent(in) :: nd1_tyy
       integer(c_int), dimension(*), intent(in) :: nd1_tyz
       type (c_ptr), value, intent(in) :: cptr_idmat1
       real(kind=c_double), intent(in) :: ca
       type (c_ptr), value, intent(in) :: cptr_drti1
       type (c_ptr), value, intent(in) :: cptr_drth1
       type (c_ptr), value, intent(in) :: cptr_damp1_x
       type (c_ptr), value, intent(in) :: cptr_damp1_y
       type (c_ptr), value, intent(in) :: cptr_clamda
       type (c_ptr), value, intent(in) :: cptr_cmu
       type (c_ptr), value, intent(in) :: cptr_epdt
       type (c_ptr), value, intent(in) :: cptr_qwp
       type (c_ptr), value, intent(in) :: cptr_qws
       type (c_ptr), value, intent(in) :: cptr_qwt1
       type (c_ptr), value, intent(in) :: cptr_qwt2
       type (c_ptr), value, intent(in) :: cptr_dxh1
       type (c_ptr), value, intent(in) :: cptr_dyh1
       type (c_ptr), value, intent(in) :: cptr_dzh1
       type (c_ptr), value, intent(in) :: cptr_dxi1
       type (c_ptr), value, intent(in) :: cptr_dyi1
       type (c_ptr), value, intent(in) :: cptr_dzi1
       type (c_ptr), value, intent(in) :: cptr_t1xx
       type (c_ptr), value, intent(in) :: cptr_t1xy
       type (c_ptr), value, intent(in) :: cptr_t1xz
       type (c_ptr), value, intent(in) :: cptr_t1yy
       type (c_ptr), value, intent(in) :: cptr_t1yz
       type (c_ptr), value, intent(in) :: cptr_t1zz
       type (c_ptr), value, intent(in) :: cptr_qt1xx
       type (c_ptr), value, intent(in) :: cptr_qt1xy
       type (c_ptr), value, intent(in) :: cptr_qt1xz
       type (c_ptr), value, intent(in) :: cptr_qt1yy
       type (c_ptr), value, intent(in) :: cptr_qt1yz
       type (c_ptr), value, intent(in) :: cptr_qt1zz
       type (c_ptr), value, intent(in) :: cptr_t1xx_px
       type (c_ptr), value, intent(in) :: cptr_t1xy_px
       type (c_ptr), value, intent(in) :: cptr_t1xz_px
       type (c_ptr), value, intent(in) :: cptr_t1yy_px
       type (c_ptr), value, intent(in) :: cptr_qt1xx_px
       type (c_ptr), value, intent(in) :: cptr_qt1xy_px
       type (c_ptr), value, intent(in) :: cptr_qt1xz_px
       type (c_ptr), value, intent(in) :: cptr_qt1yy_px
       type (c_ptr), value, intent(in) :: cptr_t1xx_py
       type (c_ptr), value, intent(in) :: cptr_t1xy_py
       type (c_ptr), value, intent(in) :: cptr_t1yy_py
       type (c_ptr), value, intent(in) :: cptr_t1yz_py
       type (c_ptr), value, intent(in) :: cptr_qt1xx_py
       type (c_ptr), value, intent(in) :: cptr_qt1xy_py
       type (c_ptr), value, intent(in) :: cptr_qt1yy_py
       type (c_ptr), value, intent(in) :: cptr_qt1yz_py
       type (c_ptr), intent(inout) :: cptr_v1x
       type (c_ptr), intent(inout) :: cptr_v1y
       type (c_ptr), intent(inout) :: cptr_v1z
       integer(c_int), intent(in) :: nxb2
       integer(c_int), intent(in) :: nyb2
       integer(c_int), intent(in) :: nxbtm
       integer(c_int), intent(in) :: nybtm
       integer(c_int), intent(in) :: nzbtm
       integer(c_int), intent(in) :: mw2_pml
       integer(c_int), intent(in) :: mw2_pml1
       integer(c_int), dimension(*), intent(in) :: nd2_txy
       integer(c_int), dimension(*), intent(in) :: nd2_txz
       integer(c_int), dimension(*), intent(in) :: nd2_tyy
       integer(c_int), dimension(*), intent(in) :: nd2_tyz
       type (c_ptr), value, intent(in) :: cptr_idmat2
       type (c_ptr), value, intent(in) :: cptr_drti2
       type (c_ptr), value, intent(in) :: cptr_drth2
       type (c_ptr), value, intent(in) :: cptr_damp2_x
       type (c_ptr), value, intent(in) :: cptr_damp2_y
       type (c_ptr), value, intent(in) :: cptr_damp2_z
       type (c_ptr), value, intent(in) :: cptr_t2xx
       type (c_ptr), value, intent(in) :: cptr_t2xy
       type (c_ptr), value, intent(in) :: cptr_t2xz
       type (c_ptr), value, intent(in) :: cptr_t2yy
       type (c_ptr), value, intent(in) :: cptr_t2yz
       type (c_ptr), value, intent(in) :: cptr_t2zz
       type (c_ptr), value, intent(in) :: cptr_qt2xx
       type (c_ptr), value, intent(in) :: cptr_qt2xy
       type (c_ptr), value, intent(in) :: cptr_qt2xz
       type (c_ptr), value, intent(in) :: cptr_qt2yy
       type (c_ptr), value, intent(in) :: cptr_qt2yz
       type (c_ptr), value, intent(in) :: cptr_qt2zz
       type (c_ptr), value, intent(in) :: cptr_dxh2
       type (c_ptr), value, intent(in) :: cptr_dyh2
       type (c_ptr), value, intent(in) :: cptr_dzh2
       type (c_ptr), value, intent(in) :: cptr_dxi2
       type (c_ptr), value, intent(in) :: cptr_dyi2
       type (c_ptr), value, intent(in) :: cptr_dzi2
       type (c_ptr), value, intent(in) :: cptr_t2xx_px
       type (c_ptr), value, intent(in) :: cptr_t2xy_px
       type (c_ptr), value, intent(in) :: cptr_t2xz_px
       type (c_ptr), value, intent(in) :: cptr_t2yy_px
       type (c_ptr), value, intent(in) :: cptr_t2xx_py
       type (c_ptr), value, intent(in) :: cptr_t2xy_py
       type (c_ptr), value, intent(in) :: cptr_t2yy_py
       type (c_ptr), value, intent(in) :: cptr_t2yz_py
       type (c_ptr), value, intent(in) :: cptr_t2xx_pz
       type (c_ptr), value, intent(in) :: cptr_t2xz_pz
       type (c_ptr), value, intent(in) :: cptr_t2yz_pz
       type (c_ptr), value, intent(in) :: cptr_t2zz_pz
       type (c_ptr), value, intent(in) :: cptr_qt2xx_px
       type (c_ptr), value, intent(in) :: cptr_qt2xy_px
       type (c_ptr), value, intent(in) :: cptr_qt2xz_px
       type (c_ptr), value, intent(in) :: cptr_qt2yy_px
       type (c_ptr), value, intent(in) :: cptr_qt2xx_py
       type (c_ptr), value, intent(in) :: cptr_qt2xy_py
       type (c_ptr), value, intent(in) :: cptr_qt2yy_py
       type (c_ptr), value, intent(in) :: cptr_qt2yz_py
       type (c_ptr), value, intent(in) :: cptr_qt2xx_pz
       type (c_ptr), value, intent(in) :: cptr_qt2xz_pz
       type (c_ptr), value, intent(in) :: cptr_qt2yz_pz
       type (c_ptr), value, intent(in) :: cptr_qt2zz_pz
       type (c_ptr), intent(inout) :: cptr_v2x
       type (c_ptr), intent(inout) :: cptr_v2y
       type (c_ptr), intent(inout) :: cptr_v2z
       integer(c_int), intent(in) :: myid
    end subroutine compute_stressCDebug

    subroutine free_device_memC(lbx, lby) bind(c, name='free_device_memC')
       use, intrinsic :: iso_c_binding, ONLY: C_INT, C_PTR
       integer(c_int), dimension(*), intent(in) :: lbx
       integer(c_int), dimension(*), intent(in) :: lby
    end subroutine free_device_memC

    subroutine one_time_data_vel (index_xyz_source, ixsX, ixsY, ixsZ, &
                    famp, fampX, fampY, ruptm, ruptmX, riset, risetX, sparam2, &
                    sparam2X )&
        bind(C, name="one_time_data_vel")
        use:: iso_c_binding
        type(c_ptr), intent(in), value :: index_xyz_source, famp, ruptm, riset, sparam2
        integer(c_int), value, intent(in):: ixsX, ixsY, ixsZ, fampX, fampY,&
        ruptmX, risetX, sparam2X
    end subroutine one_time_data_vel

end interface

 integer, intent(IN):: myid_world
 character (len=72):: fileSou,file_tmp
 character (len=72):: ch3_run,ch3_proc 
 character (len=72):: fname_list='FD_output_file_list'
 integer :: nch2,nlch,nrc3,nstm,ntprt,inne,ipt,ierr, ierrt
 integer :: i,ii,jj,kk,krun,is_moment,getlen,maxpr,deviceID;
 double precision :: tm0,tm1,tim,vp,vs,den,cl,sm
 double precision :: tm2(2), tm3, tm4, tmtmp
 integer:: iteration=0
 real(c_double) :: tstart, tend, iter_tstart, iter_tend, &
                    total_tstart,total_tend
 character(len=10) :: int2str
!
 maxpr=3
 if(myid_world >= nproc) return
 if(myid == 0 ) then
   open(unit=9,file=fname_list,status='replace')
   write(9,*) num_fout,num_src_model,recv_type
 endif
 nrc3=3*nrecs
 do krun=1,num_src_model
!------------------------------------------------------------------
! Inputing locations and parameters for this seismic source 
!------------------------------------------------------------------
   fileSou=source_name_list(krun)
   call input_source_param(fileSou,group_id,myid,dt_fd,xref_fdm, &
                           yref_fdm,angle_north_to_x,xref_src, &
                           yref_src,afa_src,is_moment,ierr)
! write(*,*) ' after call input_source_param'
   if (ierr /= 0) call MPI_FINALIZE(ierr)
!------------------------------------------------------------------
! Setting initial values for FD calculation 
!------------------------------------------------------------------
   call initial(myid,is_moment,dt_fd)
!    write(*,*) ' after call initial'
   call MPI_Barrier(group_id, ierr)
!------------------------------------------------------------------
! Creat the list of outputfile
!------------------------------------------------------------------
   nch2= getlen(fd_out_root)
   call dig2ch(krun,maxpr,file_tmp)
   ch3_run(1:maxpr+1)='.'//file_tmp(1:maxpr)
   if(myid == 0) write(9, *) 'The output file names of ', &
                 'FD simulation using source model ',krun 
   fd_out_file=''
   do i=1,nproc
     if(nfiles(i) > 0) then
       call dig2ch(i,3,file_tmp)
       ch3_proc(1:4)='.'//file_tmp(1:3)
       file_tmp=fd_out_root(1:nch2)//ch3_proc(1:4)//ch3_run(1:4)
       nlch=nch2+8
       if(myid == i-1) fd_out_file=file_tmp(1:nlch)
       if(myid == 0) write(9,'(1a)') file_tmp(1:nlch)
     endif
   enddo
!------------------------------------------------------------------
!  Open a file to store outputting results
!------------------------------------------------------------------
   if(nrecs > 0) then
     open(21, file=fd_out_file, form='unformatted', status='replace')
     write(21) recv_type,sum(nfiles),nrecs,npt_out,dt_out
     if(recv_type ==1 ) then
       do i=1,nrecs
         write(21) fname_stn(i)
       enddo
     else
       write(21) nblock
       do i=1,nblock
         write(21) nrxyz(:,i)
       enddo
       call Type2_Rev_Location(nrc3,syn_dti)
       write(21) syn_dti
     endif
   endif
!   write(*,*) ' after Creat the list of outputfile'
   call MPI_Barrier(group_id, ierr)
!------------------------------------------------------------------
!     loop over all time steps
!------------------------------------------------------------------

! initialise loggers
   call init_logging(myid)
    
   nstm=10
   deviceID = 0;
   !allocate all gpu memory by calling a funciton in the .h file
   call set_deviceC(deviceID)
   if(allocated(index_xyz_source) .and. &
        allocated(famp) .and. &
        allocated(ruptm) .and. &
        allocated(riset) .and. &
        allocated(sparam2)) then
       call one_time_data_vel (c_loc(index_xyz_source), size(index_xyz_source,1), & 
                    size(index_xyz_source,2), size(index_xyz_source,3), &
                    c_loc(famp), size(famp,1), size(famp,2), &
                    c_loc(ruptm), size(ruptm, 1), c_loc(riset), size(riset,1), &
                    c_loc(sparam2), size(sparam2,1))
   endif

   include 'allocate_gpu_mem.h'
   call init_gpuarr_metadata(nxtop, nytop, nztop, nxbtm, nybtm, nzbtm)
   call init_metadata()
   include 'copy_inputs_to_gpu.h'
!  include 'copy_outputs_to_gpu.h'
!

   write(int2str,'(i10)') npt_out*intprt
   call log_string( "Number of Iterations "//trim(int2str))

   call record_time(total_tstart)
 write(*,*) "writing to file"
 open(unit=51,file='Vel_X.out22',status='replace',position='rewind',form='formatted')
 write(51,*) "Vel_inner_X_I"
 write(51,60) syn_dti
 close(51)  
   !npt_out=3
   !intprt=3
   do ntprt=1,npt_out
   do inne=1,intprt
     iteration=iteration+1
     ipt=(ntprt-1)*intprt+inne
     tim=(ipt-0.5)*dt_fd
     
     call record_time(iter_tstart)
     write(int2str,'(i10)') iteration
      call log_string( "++++++++++++++ITERATION "//trim(int2str)//"++++++++++++++")

     !XSC: change the update_velocity function into two
     !call update_velocity(group_id,myid)

! To use all Fortran uncommment the line below and comment out 
! the include call to the C routine a couple lines down
!     call compute_velocity
!
! DRHO: The C call argument list is so long, I use an include so it is easy to comment out
!
! Call the C version of the velocity calculation
!    include 'copy_outputs_from_gpu.h'

     call record_time(tstart)
     include 'call_computevelocityc.h'
     !call compute_velocityCDummy (nxtop, nztop, nytop, nxbtm, nzbtm, &
     !nybtm, cptr_v1x, cptr_v2x,  cptr_v1y, cptr_v2y,    cptr_v1z,  cptr_v2z) 
     !include 'copy_outputs_to_gpu.h'
     call record_time(tend)
     call log_timing("compute_velocity", tend-tstart)

     call record_time(tstart)
     call comm_velocity(group_id, myid)
     call record_time(tend)
     call log_timing("comm_velocity", tend-tstart)
!!
! Fortran MPI data exchange routines
     call record_time(tstart)
     call add_dcs(tim)
     call record_time(tend)
     call log_timing ("add_dcs", tend- tstart)

     call record_time(tstart)
     include 'call_computestressc.h'
     call record_time(tend)
     call log_timing("compute_stress", tend-tstart)

!!     call compute_stress(myid, tim)
!
     call record_time(tstart)
     call comm_stress(myid, group_id)
     call record_time(tend)
     call log_timing("comm_stress", tend-tstart)

     call record_time(tstart)
     call cpy_d2h_velocityOutputsC ( cptr_v1x, cptr_v1y, cptr_v1z, &
                   cptr_v2x, cptr_v2y, cptr_v2z, &
                   nxtop, nytop, nztop, &
                   nxbtm, nybtm, nzbtm)
     call record_time(tend)
     call log_timing("Output Write Data Transfer", tend-tstart)

     if(inne==1 .and. nrecs>0) then
       if(recv_type ==1 ) then
         call output_1(nrc3,syn_dti)
       else
         call output_2(nrc3,syn_dti)
       endif
       write(21) syn_dti
     endif
    call record_time(iter_tend)
    call log_timing("Iteration", iter_tend-iter_tstart)
   enddo
   enddo

   call record_time(total_tend)
   call log_timing("full-loop", total_tend-total_tstart)

 write(*,*) "writing to file"
 open(unit=51,file='Vel_X.out',status='replace',position='rewind',form='formatted')
 write(51,*) "Vel_inner_X_I"
 write(51,60) syn_dti
 close(51)  
 60 format(E20.10)
   call free_device_memC(lbx, lby)
!------------------------------------------------------------------
!  Close the output files
!------------------------------------------------------------------
   if(nrecs > 0) then
     close(21)
   endif
   call MPI_Barrier(group_id, ierr)
 enddo
!
 if(myid == 0) then
   write(9,*) 
   write(9,*) 'The number of processors =',nproc
   write(9,'(5i9)') (nfiles(i),i=1,nproc)
   close(9)
 endif
 
end subroutine run_fd_simul
!
!+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
subroutine get_time(millis)
! This subroutine gets the computer time (minute)
 implicit NONE
 double precision, intent(OUT):: millis
 character(len=8)::  date
 character(len=10):: time
 character(len=5)::  tzone
 integer, dimension(8):: values
 integer::   year, mes, dia, hora, minute, sec, msec
!
 call date_and_time(date,time,tzone,values)
 year    = values(1)
 mes     = values(2)
 dia     = values(3)
 hora    = values(5)
 minute  = values(6)
 sec     = values(7)
 msec    = values(8)
 !millis  = (dia*24.0+hora)*60.0*60*1000+minute*60.0*1000+(sec*1000.0+msec)
 millis  = minute*60.0*1000+(sec*1000.0+msec)
 return
end subroutine get_time

