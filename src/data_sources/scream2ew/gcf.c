/*
 * gcf.c:
 *
 * Copyright (c) 2003 Guralp Systems Limited
 * Author James McKenzie, contact <software@guralp.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/*
 * $Log$
 * Revision 1.4  2006/09/06 21:37:21  paulf
 * modifications made for InjectSOH
 *
 * Revision 1.3  2006/05/02 16:25:59  paulf
 *  new installment of scream2ew from GSL
 *
 * Revision 1.1  2003/03/27 18:07:18  alex
 * Initial revision
 *
 * Revision 1.4  2003/02/28 17:05:37  root
 * #
 *
 */

#include "project.h"
#include "gcf.h"
#include "gputil.h"


static time_t
gcf_time_to_time_t (gcf_time * t)
{
  time_t ret = GCF_EPOCH;

  ret += (t->day) * 24 * 60 * 60;
  ret += (t->sec);

  return ret;
}

static int
extract_8 (gcf_block b)
{
  uint8_t *ptr = b->buf;
  int *optr = b->data;

  int32_t val;
  int n = b->samples;

  ptr += 16;

  val = gp_int32 (ptr);
  ptr += 4;

  if (*ptr) {
    warning(("First difference is not zero"));
    return 1;
  }
  while (n--)
    {
      val += gp_int8 (ptr);
      ptr++;
      *(optr++) = val;
    }
  b->fic = val;
  b->ric = gp_int32 (ptr);

  if (b->fic != b->ric) {
    warning(("Fic!=Ric"));
    return 1;
  }

  return 0;
}

static int
extract_16 (gcf_block b)
{
  uint8_t *ptr = b->buf;
  int *optr = b->data;

  int32_t val;
  int n = b->samples;

  ptr += 16;

  val = gp_int32 (ptr);
  ptr += 4;

  if (*ptr) {
    warning(("First difference is not zero"));
    return 1;
  }

  while (n--)
    {
      val += gp_int16 (ptr);
      ptr += 2;
      *(optr++) = val;
    }
  b->fic = val;
  b->ric = gp_int32 (ptr);

  if (b->fic != b->ric) {
    warning(("Fic!=Ric"));
    return 1;
  }
  
  return 0;
}

/* 24-bit data is a special case: the arithmetic has to be done modulo 2^24, */
/* not modulo 2^32, as for the other formats.                                */
/* See http://www.guralp.com/apps/ok?doc=GCF_Intro                           */

static int
extract_24 (gcf_block b)
{
  uint8_t *ptr = b->buf;
  int *optr = b->data;

  int32_t val;
  int n = b->samples;

  ptr += 16;

  val = gp_int32 (ptr);
  ptr += 4;

  if ( ( ( val & 0xFF800000 ) != 0 ) &&
       ( ( val & 0xFF800000 ) != 0xFF800000 ) ) {
    warning(("claimed 24 bit data isn't"));
    return 1;
  }

  if (gp_int24 (ptr)) {
    warning(("First difference is not zero"));
    return 1;
  }

  /* Summation must be done using 24-bit arithmetic, i.e., modulo 2^24 with */
  /* silent overflow and underflow.  Without a 24-bit int datatype, we use  */
  /* 32 bit int summands, left shifting them (multiplying by 256) into the  */
  /* upper 24 bits of a 32-bit int for the modular arithmetic.  The sum is  */
  /* then arithmetically right shifted and sign extended (divided by 256)   */
  /* into the proper position for the int output buffer.                    */

  /* Note: >> cannot be used for the arithmetic right shift since the C90   */
  /* standard (6.3.7) says whether >> is a logical or arithmetic shift for  */
  /* negative signed int values is implementation-defined.  Thus, >> does   */
  /* not guarantee a sign-extended result.  We use * 256 instead of << 8    */
  /* for the left shift to be consistent with the use of / 256 instead of   */
  /* >> 8 for the right shift.  Optimizing compilers generate nearly the    */
  /* same code either way.  (Integer division may or may round.  LLVM and   */
  /* GCC both round towards zero, which adds a couple unnecessary instruc-  */
  /* tions for us.)                                                         */

  val *= 256;
  while (n--)
    {
      val += gp_int24 (ptr) * 256;
      ptr += 3;
      *(optr++) = val / 256;
    }
  val /= 256;

  if ( ( ( val & 0xFF800000 ) != 0 ) &&
       ( ( val & 0xFF800000 ) != 0xFF800000 ) ) {
    warning(("claimed 24 bit data isn't"));
    return 1;
  }

  b->fic = val;
  b->ric = gp_int32 (ptr);

  if (b->fic != b->ric) {
    warning(("Fic!=Ric"));
    return 1;
  }

  return 0;
}

static int
extract_32 (gcf_block b)
{
  uint8_t *ptr = b->buf;
  int *optr = b->data;

  int32_t val;
  int n = b->samples;

  ptr += 16;

  val = gp_int32 (ptr);
  ptr += 4;

  if (gp_int32 (ptr)) {
    warning(("First difference is not zero"));
    return 1;
  }

  while (n--)
    {
      val += gp_int32 (ptr);
      ptr += 4;
      *(optr++) = val;
    }
  b->fic = val;
  b->ric = gp_int32 (ptr);

  if (b->fic != b->ric) {
    warning(("Fic!=Ric"));
    return 1;
  }
  
  return 0;
}

int
gcf_dispatch (uint8_t * buf, int sz)
{
  int i;
  struct gcf_block_struct block;
  int err = 0;
  int t_offset_numerator;                  /* RL 2016 */
  int t_offset_denominator;                /* RL 2016 */
	
  block.buf = buf;
  block.size = sz;
  block.csize = 0;

  strcpy (block.sysid, gp_base36_to_a (gp_uint32 (buf)));
  strcpy (block.strid, gp_base36_to_a (gp_uint32 (buf + 4)));

  i = gp_uint16 (buf + 8);
  block.start.day = i >> 1;
  i &= 1;
  i <<= 16;
  block.start.sec = i | gp_uint16 (buf + 10);

  block.estart = gcf_time_to_time_t (&block.start);
  
  block.sample_rate = buf[13];

  /* Special case for sps>250 or sps<1 - only one byte used so can't have these.  */
  /* Unlikely possible values used to signify likely impossible values (RL 2016). */
  /* From table 3.1 in "SWA-RFC-GCFR-Guralp Compressed Format - Reference.pdf"    */

  t_offset_denominator = 0;
  switch ( buf[13] ) {
    case 157:
	  block.sample_rate=0.1;
	  break;
    case 161:
	  block.sample_rate=0.125;
	  break;
    case 162:  
	  block.sample_rate=0.2;
	  break;
    case 164:
	  block.sample_rate=0.25;
	  break;
    case 167:
	  block.sample_rate=0.5;
	  break;
    case 171:
	  block.sample_rate=400;
	  t_offset_denominator = 8;
	  break;
    case 174:
	  block.sample_rate=500;
	  t_offset_denominator = 2;
	  break;
    case 176:
	  block.sample_rate=1000;
	  t_offset_denominator = 4;
	  break;
    case 179:
	  block.sample_rate=2000;
	  t_offset_denominator = 8;
	  break;
    case 181:
	  block.sample_rate=4000;
	  t_offset_denominator = 16;
	  break;
  }

  /* For higher sample rates need more than one block for a second */
  /* This means block.estart needs an offset from the nearest second */
  /* The numerator is stored in the top 4 bits of byte 14 */
  /* The denominator comes from the same table as the sample rate. (RL, 2016) */
  if ( t_offset_denominator != 0 ) {
    t_offset_numerator = (buf[14] & 240) >> 4;
	block.estart_offset = (double)t_offset_numerator/(double)t_offset_denominator;
  }
  else {
    block.estart_offset = 0;
  }
  
  block.format = buf[14] & 7;
  block.records = buf[15];
  block.samples = (block.format) * (block.records);

  block.text = buf + 16;

  if ( block.sample_rate != 0.0 )
    {
      switch (block.format)
        {
        case 4:
          block.csize = 24 + block.samples;
          err = extract_8 (&block);
          break;
        case 2:
          block.csize = 24 + (2 * block.samples);
          err = extract_16 (&block);
          break;
        case 1:
          if ((sz - 24) == (4 * block.samples))
            {
              block.csize = 24 + (4 * block.samples);
              err = extract_32 (&block);
            }
          else if ((sz - 24) == (3 * block.samples))
            {
              block.csize = 24 + (3 * block.samples);
              err = extract_24 (&block);
            }
          else
            {
              /* Guess 32 */
              err = extract_32 (&block);
            }
          break;
        default:
          warning(("unknown GCF compression format"));
          err = 1;
        }
    }

  if(!err) dispatch (&block);
  return err;
}
