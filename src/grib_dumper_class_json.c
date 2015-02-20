/*
 * Copyright 2005-2015 ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 *
 * In applying this licence, ECMWF does not waive the privileges and immunities granted to it by
 * virtue of its status as an intergovernmental organisation nor does it submit to any jurisdiction.
 */

#include "grib_api_internal.h"
#include <ctype.h>
/*
   This is used by make_class.pl

   START_CLASS_DEF
   CLASS      = dumper
   IMPLEMENTS = dump_long;dump_bits
   IMPLEMENTS = dump_double;dump_string;dump_string_array
   IMPLEMENTS = dump_bytes;dump_values
   IMPLEMENTS = dump_label;dump_section
   IMPLEMENTS = init;destroy
   MEMBERS = long section_offset
   MEMBERS = long begin
   MEMBERS = long empty
   MEMBERS = long end
   END_CLASS_DEF

 */


/* START_CLASS_IMP */

/*

Don't edit anything between START_CLASS_IMP and END_CLASS_IMP
Instead edit values between START_CLASS_DEF and END_CLASS_DEF
or edit "dumper.class" and rerun ./make_class.pl

*/

static void init_class      (grib_dumper_class*);
static int init            (grib_dumper* d);
static int destroy         (grib_dumper*);
static void dump_long       (grib_dumper* d, grib_accessor* a,const char* comment);
static void dump_bits       (grib_dumper* d, grib_accessor* a,const char* comment);
static void dump_double     (grib_dumper* d, grib_accessor* a,const char* comment);
static void dump_string     (grib_dumper* d, grib_accessor* a,const char* comment);
static void dump_string_array     (grib_dumper* d, grib_accessor* a,const char* comment);
static void dump_bytes      (grib_dumper* d, grib_accessor* a,const char* comment);
static void dump_values     (grib_dumper* d, grib_accessor* a);
static void dump_label      (grib_dumper* d, grib_accessor* a,const char* comment);
static void dump_section    (grib_dumper* d, grib_accessor* a,grib_block_of_accessors* block);

typedef struct grib_dumper_json {
    grib_dumper          dumper;  
/* Members defined in json */
	long section_offset;
	long begin;
	long empty;
	long end;
} grib_dumper_json;


static grib_dumper_class _grib_dumper_class_json = {
    0,                              /* super                     */
    "json",                              /* name                      */
    sizeof(grib_dumper_json),     /* size                      */
    0,                                   /* inited */
    &init_class,                         /* init_class */
    &init,                               /* init                      */
    &destroy,                            /* free mem                       */
    &dump_long,                          /* dump long         */
    &dump_double,                        /* dump double    */
    &dump_string,                        /* dump string    */
    &dump_string_array,                        /* dump string array   */
    &dump_label,                         /* dump labels  */
    &dump_bytes,                         /* dump bytes  */
    &dump_bits,                          /* dump bits   */
    &dump_section,                       /* dump section      */
    &dump_values,                        /* dump values   */
    0,                             /* header   */
    0,                             /* footer   */
};

grib_dumper_class* grib_dumper_class_json = &_grib_dumper_class_json;

/* END_CLASS_IMP */

GRIB_INLINE static int grib_inline_strcmp(const char* a,const char* b) {
    if (*a != *b) return 1;
    while((*a!=0 && *b!=0) &&  *(a) == *(b) ) {a++;b++;}
    return (*a==0 && *b==0) ? 0 : 1;
}

static int depth=2;

static void init_class      (grib_dumper_class* c){}

static int  init(grib_dumper* d)
{
    grib_dumper_json *self = (grib_dumper_json*)d;
    self->section_offset=0;
    self->empty=1;

    return GRIB_SUCCESS;
}

static int  destroy  (grib_dumper* d)
{
    return GRIB_SUCCESS;
}

static void print_accessor(grib_accessor* a,FILE* out) {
  size_t len=1;
  long lval;
  double dval;
  char* cval=NULL;
  switch (grib_accessor_get_native_type(a)) {
    case GRIB_TYPE_LONG:
      grib_unpack_long(a,&lval,&len);
      fprintf(out,"%ld",lval);
      break;
    case GRIB_TYPE_DOUBLE:
      grib_unpack_double(a,&dval,&len);
      fprintf(out,"%g",dval);
      break;
    case GRIB_TYPE_STRING:
      len=grib_string_length(a)+1;
      cval=grib_context_malloc_clear(a->parent->h->context,len);
      grib_unpack_string(a,cval,&len);
      fprintf(out,"\"%s\"",cval);
      grib_context_free(a->parent->h->context,cval);
      break;
  }
}

static void dump_attributes(grib_dumper* d,grib_accessor* a,FILE* out) {
  int i=0;
  int has_attributes=0;
  while (a->attributes[i] || i > MAX_ACCESSOR_ATTRIBUTES) {
    has_attributes=grib_accessor_has_attributes(a->attributes[i]);
    fprintf(out,",\"%s\":",a->attributes[i]->name);
    if (has_attributes) {
      fprintf(out," { \"key\" : \"%s\", \"value\" : ",a->attributes[i]->name);
      print_accessor(a->attributes[i],out);
      dump_attributes(d,a->attributes[i],out);
      fprintf(out,"}\n");
    } else {
      print_accessor(a->attributes[i],out);
    }
    i++;
  }
}

static void dump_values(grib_dumper* d,grib_accessor* a)
{
    grib_dumper_json *self = (grib_dumper_json*)d;
    double value; size_t size = 1;
    double *values=NULL;
    int err = 0;
    int i,tab;
    long count=0;
    int mydepth=depth+2;
    double missing_value = GRIB_MISSING_DOUBLE;


    grib_value_count(a,&count);
    size=count;

    if ( (a->flags & GRIB_ACCESSOR_FLAG_DUMP) == 0)
        return;

    if (size>1) {
        values=(double*)grib_context_malloc_clear(a->parent->h->context,sizeof(double)*size);
        err=grib_unpack_double(a,values,&size);
    } else {
        err=grib_unpack_double(a,&value,&size);
    }

    if (self->begin==0 && self->empty==0 ) fprintf(self->dumper.out,",\n");
    else self->begin=0;

    self->empty=0;
    fprintf(self->dumper.out,"%-*s",mydepth," ");
    fprintf(self->dumper.out,"{\"key\": \"%s\", \"value\": ",a->name);

    err = grib_set_double(a->parent->h, "missingValue", missing_value);
    if (size>1) {
        int cols=4;
        int count=0;
        int lens=strlen(a->name);
        fprintf(self->dumper.out,"[ ");
        tab=lens+mydepth+7;

        for (i=0; i<size-1; ++i) {
            if (count>cols || i==0) {fprintf(self->dumper.out,"\n%-*s",tab," ");count=0;}
            if (values[i] == missing_value)
                fprintf(self->dumper.out, "%s, ", "null");
            else
                fprintf(self->dumper.out, "%g, ", values[i]);
            count++;
        }
        if (count>cols) fprintf(self->dumper.out,"\n%-*s",tab," ");
        if (grib_is_missing_double(a,values[i]))
            fprintf(self->dumper.out, "%s ","null");
        else
            fprintf(self->dumper.out, "%g ",values[i]);

        tab=lens+mydepth+5;
        fprintf(self->dumper.out,"\n%-*s] ",tab," ");
        grib_context_free(a->parent->h->context,values);
    } else {
        if( grib_is_missing_double(a,value) )
            fprintf(self->dumper.out," null");
        else
            fprintf(self->dumper.out," %g",value);
    }
    dump_attributes(d,a,self->dumper.out);

    fprintf(self->dumper.out," }");

}


static void dump_long(grib_dumper* d,grib_accessor* a,const char* comment)
{
    grib_dumper_json *self = (grib_dumper_json*)d;
    long value; size_t size = 1;
    long *values=NULL;
    int err = 0;
    int i,tab;
    long count=0;
    int mydepth=depth+2;

    grib_value_count(a,&count);
    size=count;

    if ( (a->flags & GRIB_ACCESSOR_FLAG_DUMP) == 0)
        return;

    if (size>1) {
        values=(long*)grib_context_malloc_clear(a->parent->h->context,sizeof(long)*size);
        err=grib_unpack_long(a,values,&size);
    } else {
        err=grib_unpack_long(a,&value,&size);
    }

    if (self->begin==0 && self->empty==0) fprintf(self->dumper.out,",\n");
    else self->begin=0;

    self->empty=0;

    fprintf(self->dumper.out,"%-*s",mydepth," ");
    fprintf(self->dumper.out,"{\"key\": \"%s\", \n",a->name);

    if (size>1) {
        int cols=9;
        int count=0;
        int lens=strlen(a->name);
        fprintf(self->dumper.out,"%-*s",mydepth," ");
        fprintf(self->dumper.out,"\"value\" : [");
        tab=lens+mydepth+7;
        for (i=0;i<size-1;i++) {
            if (count>cols || i==0) {fprintf(self->dumper.out,"\n%-*s",tab," ");count=0;}
            if (grib_is_missing_long(a,values[i])) {
              fprintf(self->dumper.out,"null, ");
	    } else {
              fprintf(self->dumper.out,"%ld, ",values[i]);
	    }
            count++;
        }
        if (count>cols) fprintf(self->dumper.out,"\n%-*s",tab," ");
        fprintf(self->dumper.out,"%ld ",values[i]);

        tab=lens+mydepth+5;
        fprintf(self->dumper.out,"\n%-*s] ",tab," ");
        grib_context_free(a->parent->h->context,values);
    } else {
        fprintf(self->dumper.out,"%-*s",mydepth," ");
        fprintf(self->dumper.out,"\"value\" : ");
        if( grib_is_missing_long(a,value) )
            fprintf(self->dumper.out,"null");
        else
            fprintf(self->dumper.out,"%ld",value);
    }
    dump_attributes(d,a,self->dumper.out);

    fprintf(self->dumper.out," }");

}

static void dump_bits(grib_dumper* d,grib_accessor* a,const char* comment)
{
}

static void dump_double(grib_dumper* d,grib_accessor* a,const char* comment)
{
    grib_dumper_json *self = (grib_dumper_json*)d;
    double value; size_t size = 1;
    int mydepth=depth+2;

    grib_unpack_double(a,&value,&size);
    if ( (a->flags & GRIB_ACCESSOR_FLAG_DUMP) == 0)
        return;

    if (self->begin==0 && self->empty==0) fprintf(self->dumper.out,",\n");
    else self->begin=0;

    self->empty=0;

    fprintf(self->dumper.out,"%-*s",mydepth," ");
    fprintf(self->dumper.out,"{\"key\": \"%s\", \"value\": ",a->name);

    if( grib_is_missing_double(a,value) )
        fprintf(self->dumper.out,"null");
    else
        fprintf(self->dumper.out,"%g",value);

    dump_attributes(d,a,self->dumper.out);
    fprintf(self->dumper.out," }");

}

static void dump_string_array(grib_dumper* d,grib_accessor* a,const char* comment)
{
  grib_dumper_json *self = (grib_dumper_json*)d;
  char **values;
  size_t size = 0,i=0;
  grib_context* c=NULL;
  int err = 0;
  int tab=0;
  long count=0;
  int mydepth=depth+2;

  c=a->parent->h->context;

  if ( (a->flags & GRIB_ACCESSOR_FLAG_DUMP) == 0)
    return;

  grib_value_count(a,&count);
  size=count;
  if (size==1) {
    dump_string(d,a,comment);
    return;
  }

  if (self->begin==0 && self->empty==0) fprintf(self->dumper.out,",\n");
  else self->begin=0;

  self->empty=0;

  values=(char**)grib_context_malloc_clear(c,size*sizeof(char*));
  if (!values) {
  	grib_context_log(c,GRIB_LOG_FATAL,"unable to allocate %d bytes",(int)size);
	return;
  }

  err = grib_unpack_string_array(a,values,&size);

  fprintf(self->dumper.out,"%-*s",mydepth," ");
  fprintf(self->dumper.out,"{\"key\": \"%s\", \"value\": [\n",a->name);
  tab=mydepth+1;
  for  (i=0;i<size-1;i++) {
      fprintf(self->dumper.out,"%-*s\"%s\",\n",(int)(tab+strlen(a->name)+4)," ",values[i]);
  }
  fprintf(self->dumper.out,"%-*s\"%s\"\n",(int)(tab+strlen(a->name)+4)," ",values[i]);
  fprintf(self->dumper.out,"%-*s",mydepth," ");
  fprintf(self->dumper.out,"  ]");
  dump_attributes(d,a,self->dumper.out);
  fprintf(self->dumper.out,"} ");

  grib_context_free(c,values);
}

static void dump_string(grib_dumper* d,grib_accessor* a,const char* comment)
{
    grib_dumper_json *self = (grib_dumper_json*)d;
    char *value=NULL;
    char *p = NULL;
    size_t size = 0;
    grib_context* c=NULL;
  int err = _grib_get_string_length(a,&size);
    int mydepth=depth+2;

    c=a->parent->h->context;
    if (size==0) return;

    if ( (a->flags & GRIB_ACCESSOR_FLAG_DUMP) == 0)
        return;

    value=(char*)grib_context_malloc_clear(c,size);
    if (!value) {
        grib_context_log(c,GRIB_LOG_FATAL,"unable to allocate %d bytes",(int)size);
        return;
    }

    if (self->begin==0 && self->empty==0) fprintf(self->dumper.out,",\n");
    else self->begin=0;

    self->empty=0;

    err = grib_unpack_string(a,value,&size);
    p=value;

    while(*p) { if(!isprint(*p)) *p = '.'; p++; }

    fprintf(self->dumper.out,"%-*s",mydepth," ");
    fprintf(self->dumper.out,"{\"key\": \"%s\", \"value\": ",a->name);
    if( ((a->flags & GRIB_ACCESSOR_FLAG_CAN_BE_MISSING) != 0) && grib_is_missing_internal(a) )
        fprintf(self->dumper.out,"null");
    else
        fprintf(self->dumper.out,"\"%s\"",value);

    dump_attributes(d,a,self->dumper.out);
    fprintf(self->dumper.out,"} ");

    grib_context_free(c,value);
}

static void dump_bytes(grib_dumper* d,grib_accessor* a,const char* comment)
{
}

static void dump_label(grib_dumper* d,grib_accessor* a,const char* comment)
{
}

static void dump_section(grib_dumper* d,grib_accessor* a,grib_block_of_accessors* block)
{
    grib_dumper_json *self = (grib_dumper_json*)d;
  if (!grib_inline_strcmp(a->name,"BUFR") ||
      !grib_inline_strcmp(a->name,"GRIB") ||
      !grib_inline_strcmp(a->name,"META")
     ) {
        fprintf(self->dumper.out,"[\n");
        self->begin=1;
        self->empty=1;
        grib_dump_accessors_block(d,block);
        fprintf(self->dumper.out,"\n]\n");
  } else if (!grib_inline_strcmp(a->name,"groupNumber")) {
    depth+=2;
    if ( (a->flags & GRIB_ACCESSOR_FLAG_DUMP) == 0)
            return;
    if (!self->empty) fprintf(self->dumper.out,",\n");
    fprintf(self->dumper.out,"%-*s",depth," ");

    fprintf(self->dumper.out,"[");

    fprintf(self->dumper.out,"\n");
    /* fprintf(self->dumper.out,"%-*s",depth," "); */
    self->begin=1;
    self->empty=1;
    grib_dump_accessors_block(d,block);
    fprintf(self->dumper.out,"\n");
    fprintf(self->dumper.out,"%-*s",depth," ");
    fprintf(self->dumper.out,"]");

    depth-=2;
  } else {
        grib_dump_accessors_block(d,block);
  }
}

