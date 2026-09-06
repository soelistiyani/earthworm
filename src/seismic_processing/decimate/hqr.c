/*
 *   THIS FILE IS UNDER RCS - DO NOT MODIFY UNLESS YOU HAVE
 *   CHECKED IT OUT USING THE COMMAND CHECKOUT.
 *
 *    $Id: hqr.c 7327 2018-04-16 17:44:45Z alexander $
 *
 *    Revision history:
 *     $Log$
 *     Revision 1.0 2018/04/13 16:50:00 alexander
 *     Initial commit of EISPACK hqr() routine
 */
/*
  ORIGINAL STATEMENT:

  Purpose:

    HQR computes all eigenvalues of a real upper Hessenberg matrix
    via the QR method.

  Licensing:

    This code is distributed under the GNU LGPL license.

  Modified:

    10 February 2018

  Author:

    Original FORTRAN77 version by Smith, Boyle, Dongarra, Garbow, Ikebe,
    Klema, Moler.
    C version by John Burkardt.

  Reference:

    Martin, Peters, James Wilkinson,
    HQR,
    Numerische Mathematik,
    Volume 14, pages 219-231, 1970.

    James Wilkinson, Christian Reinsch,
    Handbook for Automatic Computation,
    Volume II, Linear Algebra, Part 2,
    Springer, 1971,
    ISBN: 0387054146,
    LC: QA251.W67.

    Brian Smith, James Boyle, Jack Dongarra, Burton Garbow,
    Yasuhiko Ikebe, Virginia Klema, Cleve Moler,
    Matrix Eigensystem Routines, EISPACK Guide,
    Lecture Notes in Computer Science, Volume 6,
    Springer Verlag, 1976,
    ISBN13: 978-3540075462,
    LC: QA193.M37.
*/

#include <math.h>

#ifndef bool
typedef enum { false, true } bool;
#endif

int r8_sign_double(double number) {
   return ((number >= 0) ? 1 : -1);
}

int i4_min (int a, int b) {
   return ((a < b) ? a : b);
}

/******************************************************************************/

int hqr ( int n, int low, int igh, double *h, double *wr, double *wi )

/******************************************************************************/
/*
  Parameters:

    Input, int N, the order of the matrix.

    Input, int LOW, IGH, two integers determined by 
    BALANC.  If BALANC is not used, set LOW=0, IGH=N-1.

    Input/output, double H[N,N], the N by N upper Hessenberg matrix.
    Information about the transformations used in the reduction to
    Hessenberg form by ELMHES or ORTHES, if performed, is stored
    in the remaining triangle under the Hessenberg matrix.
    On output, the information in H has been destroyed.

    Output, double WR[N], WI[N], the real and imaginary parts of the
    eigenvalues.  The eigenvalues are unordered, except that complex
    conjugate pairs of values appear consecutively, with the eigenvalue
    having positive imaginary part listed first.  If an error break;
    occurred, then the eigenvalues should be correct for indices
    IERR+1 through N.

    Output, int HQR, error flag.
    0, no error.
    J, the limit of 30*N iterations was reached while searching for
      the J-th eigenvalue.
*/
{
  int en;
  int enm2;
  int i;
  int ierr;
  int itn;
  int its;
  int j;
  int k;
  int l;
  int m;
  int na;
  double norm;
  bool notlas;
  double p;
  double q;
  double r;
  double s;
  double t;
  double tst1;
  double tst2;
  double w;
  double x;
  double y;
  double zz;

  ierr = 0;
  norm = 0.0;
  k = 0;
/*
  Store roots isolated by BALANC and compute matrix norm.
*/
  for ( i = 0; i < n; i++ )
  {
    for ( j = k; j < n; j++ )
    {
      norm = norm + fabs ( h[i+j*n] );
    }

    k = i;
    if ( i < low || igh < i )
    {
      wr[i] = h[i+i*n];
      wi[i] = 0.0;
    }
  }

  en = igh;
  t = 0.0;
  itn = 30 * n;
/*
  Search for next eigenvalues.
*/
  if ( igh < low )
  {
    return ierr;
  }

  its = 0;
  na = igh - 1;
  enm2 = igh - 2;
/*
  Look for a single small sub-diagonal element.
*/
  while ( true )
  {
    for ( l = en; low <= l; l-- )
    {
      if ( l == low )
      {
        break;
      }
      s = fabs ( h[l-1+(l-1)*n] ) + fabs ( h[l+l*n] );
      if ( s == 0.0 )
      {
        s = norm;
      }
      tst1 = s;
      tst2 = tst1 + fabs ( h[l+(l-1)*n] );
      if ( tst2 == tst1 )
      {
        break;
      }
    }
/*
  Form shift.
*/
    x = h[en+en*n];
/*
  One root found.
*/
    if ( l == en )
    {
      wr[en] = x + t;
      wi[en] = 0.0;
      en = na;
      if ( en < low )
      {
        return ierr;
      }
      its = 0;
      na = en - 1;
      enm2 = na - 1;
      continue;
    }

    y = h[na+na*n];
    w = h[en+na*n] * h[na+en*n];
/*
  Two roots found.
*/
    if ( l == na )
    {
      p = ( y - x ) / 2.0;
      q = p * p + w;
      zz = sqrt ( fabs ( q ) );
      x = x + t;
/*
  Real root, or complex pair.
*/
      if ( 0.0 <= q )
      {
        zz = p + fabs ( zz ) * r8_sign_double ( p );
        wr[na] = x + zz;
        if ( zz == 0.0 )
        {
          wr[en] = wr[na];
        }
        else
        {
          wr[en] = x - w / zz;
        }
        wi[na] = 0.0;
        wi[en] = 0.0;
      }
      else
      {
        wr[na] = x + p;
        wr[en] = x + p;
        wi[na] = zz;
        wi[en] = - zz;
      }

      en = enm2;

      if ( en < low )
      {
        return ierr;
      }

      its = 0;
      na = en - 1;
      enm2 = na - 1;
      continue;
    }

    if ( itn == 0 )
    {
      ierr = en;
      return ierr;
    }
/*
  Form an exceptional shift.
*/
    if ( its == 10 || its == 20 )
    {
      t = t + x;

      for ( i = low; i <= en; i++ )
      {
        h[i+i*n] = h[i+i*n] - x;
      }

      s = fabs ( h[en+na*n] ) + fabs ( h[na+enm2*n] );
      x = 0.75 * s;
      y = x;
      w = - 0.4375 * s * s;
    }

    its = its + 1;
    itn = itn - 1;
/*
  Look for two consecutive small sub-diagonal elements.
*/
    for ( m = enm2; l <= m; m-- )
    {
      zz = h[m+m*n];
      r = x - zz;
      s = y - zz;
      p = ( r * s - w ) / h[m+1+m*n] + h[m+(m+1)*n];
      q = h[m+1+(m+1)*n] - zz - r - s;
      r = h[m+2+(m+1)*n];
      s = fabs ( p ) + fabs ( q ) + fabs ( r );
      p = p / s;
      q = q / s;
      r = r / s;

      if ( m == l )
      {
        break;
      }

      tst1 = fabs ( p ) * ( fabs ( h[m-1+(m-1)*n] ) + fabs ( zz ) 
        + fabs ( h[m+1+(m+1)*n] ) );
      tst2 = tst1 + fabs ( h[m+(m-1)*n] ) * ( fabs ( q ) + fabs ( r ) );

      if ( tst2 == tst1 )
      {
        break;
      }
    }

    for ( i = m + 2; i <= en; i++ )
    {
      h[i+(i-2)*n] = 0.0;
      if ( i != m + 2 )
      {
        h[i+(i-3)*n] = 0.0;
      }
    }
/*
  Double QR step involving rows l to EN and columns M to EN.
*/
    for ( k = m; k <= na; k++ )
    {
      notlas = ( k != na );

      if ( k != m )
      {
        p = h[k+(k-1)*n];
        q = h[k+1+(k-1)*n];

        if ( notlas )
        {
          r = h[k+2+(k-1)*n];
        }
        else
        {
          r = 0.0;
        }

        x = fabs ( p ) + fabs ( q ) + fabs ( r );

        if ( x == 0.0 )
        {
          continue;
        }
        p = p / x;
        q = q / x;
        r = r / x;
      }

      s = sqrt ( p * p + q * q + r * r ) * r8_sign_double ( p );

      if ( k != m )
      {
        h[k+(k-1)*n] = - s * x;
      }
      else if ( l != m )
      {
        h[k+(k-1)*n] = - h[k+(k-1)*n];
      }

      p = p + s;
      x = p / s;
      y = q / s;
      zz = r / s;
      q = q / p;
      r = r / p;
/*
  Row modification.
*/
      if ( ! notlas )
      {
        for ( j = k; j < n; j++ )
        {
          p = h[k+j*n] + q * h[k+1+j*n];
          h[k+j*n] = h[k+j*n] - p * x;
          h[k+1+j*n] = h[k+1+j*n] - p * y;
        }

        j = i4_min ( en, k + 3 );
/*
  Column modification.
*/
        for ( i = 0; i <= j; i++ )
        {
          p = x * h[i+k*n] + y * h[i+(k+1)*n];
          h[i+k*n] = h[i+k*n] - p;
          h[i+(k+1)*n] = h[i+(k+1)*n] - p * q;
        }
      }
/*
  Row modification.
*/
      else
      {
        for ( j = k; j < n; j++ )
        {
          p = h[k+j*n] + q * h[k+1+j*n] + r * h[k+2+j*n];
          h[k+j*n] = h[k+j*n] - p * x;
          h[k+1+j*n] = h[k+1+j*n] - p * y;
          h[k+2+j*n] = h[k+2+j*n] - p * zz;
        }

        j = i4_min ( en, k + 3 );
/*
  Column modification.
*/
        for ( i = 0; i <= j; i++ )
        {
          p = x * h[i+k*n] + y * h[i+(k+1)*n] + zz * h[i+(k+2)*n];
          h[i+k*n] = h[i+k*n] - p;
          h[i+(k+1)*n] = h[i+(k+1)*n] - p * q;
          h[i+(k+2)*n] = h[i+(k+2)*n] - p * r;
        }
      }
    }
  }

  return ierr;
}
