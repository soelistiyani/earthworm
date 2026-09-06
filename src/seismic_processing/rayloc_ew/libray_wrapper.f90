   Subroutine LIBRAY_WRAPPER( c_tokenName, c_dir_in ) &
   Bind ( C, Name='libray_wrapper' )
!
!  Subroutine LIBRAY_WRAPPER is a wrapper for the C rayloc_ew to call
!  Fortran Subroutine LIBRAY.
!
!  The C function prototype is:
!
!     void libray_wrapper( const char *, const char * );
!
!  Subroutine LIBRAY_WRAPPER uses the Fortran 2003 BIND ( C ) attribute
!  and Intrinsic Module ISO_C_BINDING.
!

   Use, Intrinsic :: ISO_C_BINDING, Only: C_INT, C_CHAR, C_NULL_CHAR

   Implicit None

   Integer, Parameter :: STRLEN = 1000

!
!  Arguments to this Subroutine
!

   Character ( Kind=C_CHAR ), Intent( IN  ) :: c_tokenName(*)
   Character ( Kind=C_CHAR ), Intent( IN  ) :: c_dir_in(*)

!
!  Local variables
!

   Integer   i
   Character tokenName*(STRLEN)
   Character dir_in*(STRLEN)


!
!  Copy the incoming C strings to FORTRAN strings
!

   tokenName = ' '
   Do i = 1, STRLEN
      If ( c_tokenName(i) == C_NULL_CHAR ) Then
         Exit
      End If
      tokenName(i:i) = c_tokenName(i)
   End Do

!  i = i - 1
!  Write (*,*) '--->LIBRAY_WRAPPER: tokenName*(', i, ') = "', tokenName(1:i), '"'

   dir_in = ' '
   Do i = 1, STRLEN
      If ( c_dir_in(i) == C_NULL_CHAR ) Then
         Exit
      End If
      dir_in(i:i) = c_dir_in(i)
   End Do

!  i = i - 1
!  Write (*,*) '--->LIBRAY_WRAPPER: dir_in*(', i, ') = "', dir_in(1:i), '"'

!
!  Call Subroutine LIBRAY.
!

   Call LIBRAY( tokenName, dir_in )

   Return
   End
