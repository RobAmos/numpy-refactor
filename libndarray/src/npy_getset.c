/*
 *  npy_getset.c -
 *
 */

#include <stdlib.h>
#include <memory.h>
#include "npy_config.h"
#include "npy_api.h"
#include "npy_arrayobject.h"



NDARRAY_API int
NpyArray_SetShape(NpyArray *self, NpyArray_Dims *newdims)
{
    int nd;
    NpyArray *ret;

    ret = NpyArray_Newshape(self, newdims, NPY_CORDER);
    if (ret == NULL) {
        return -1;
    }
    if (NpyArray_DATA(ret) != NpyArray_DATA(self)) {
        Npy_XDECREF(ret);
        NpyErr_SetString(NpyExc_AttributeError,
                         "incompatible shape for a non-contiguous array");
        return -1;
    }

    /* Free old dimensions and strides */
    NpyDimMem_FREE(NpyArray_DIMS(self));
    nd = NpyArray_NDIM(ret);
    NpyArray_NDIM(self) = nd;
    if (nd > 0) {
        /* create new dimensions and strides */
        NpyArray_DIMS(self) = NpyDimMem_NEW(2 * nd);
        if (NpyArray_DIMS(self) == NULL) {
            Npy_XDECREF(ret);
            NpyErr_MEMORY;
            return -1;
        }
        NpyArray_STRIDES(self) = NpyArray_DIMS(self) + nd;
        memcpy(NpyArray_DIMS(self), NpyArray_DIMS(ret),
               nd * sizeof(npy_intp));
        memcpy(NpyArray_STRIDES(self), NpyArray_STRIDES(ret),
               nd * sizeof(npy_intp));
    }
    else {
        NpyArray_DIMS(self) = NULL;
        NpyArray_STRIDES(self) = NULL;
    }
    Npy_XDECREF(ret);
    NpyArray_UpdateFlags(self, NPY_CONTIGUOUS | NPY_FORTRAN);
    return 0;
}


NDARRAY_API int
NpyArray_SetStrides(NpyArray *self, NpyArray_Dims *newstrides)
{
    NpyArray *new;
    npy_intp numbytes = 0, offset = 0;

    if (newstrides->len != NpyArray_NDIM(self)) {
        NpyErr_SetString(NpyExc_ValueError,
                         "strides must be same length as shape");
        return -1;
    }
    new = NpyArray_BASE_ARRAY(self);
    while(NULL != NpyArray_BASE_ARRAY(new)) {
        new = NpyArray_BASE_ARRAY(new);
    }

#if 0
    /* TODO: Fix this so we can set strides on a buffer-backed array. */
    /* Get the available memory through the buffer interface on
     * new->base or if that fails from the current new
     * NOTE: PyObject_AsReadBuffer is never called during tests */
    if (new->base_obj != NULL && PyObject_AsReadBuffer(new->base_obj,
                                                       (const void **)&buf,
                                                       &buf_len) >= 0) {
        offset = NpyArray_BYTES(self) - buf;
        numbytes = buf_len - offset;
    }
#else
    if (new->base_obj != NULL) {
        NpyErr_SetString(NpyExc_ValueError,
                     "strides cannot be set on array created from a buffer.");
        return -1;
    }
#endif
    else {
        NpyErr_Clear();
        numbytes = NpyArray_MultiplyList(
                       NpyArray_DIMS(new),
                       NpyArray_NDIM(new)) * NpyArray_ITEMSIZE(new);
        offset = NpyArray_BYTES(self) - NpyArray_BYTES(new);
    }

    if (!NpyArray_CheckStrides(NpyArray_ITEMSIZE(self),
                               NpyArray_NDIM(self), numbytes,
                               offset,
                               NpyArray_DIMS(self), newstrides->ptr)) {
        NpyErr_SetString(NpyExc_ValueError,
                         "strides is not compatible with available memory");
        return -1;
    }
    memcpy(NpyArray_STRIDES(self), newstrides->ptr,
           sizeof(npy_intp) * newstrides->len);
    NpyArray_UpdateFlags(self, NPY_CONTIGUOUS | NPY_FORTRAN);
    return 0;
}

static NpyArray *
_get_part(NpyArray *self, int imag)
{
    NpyArray_Descr *type;
    int offset;

    type = NpyArray_DescrFromType(NpyArray_TYPE(self) - NPY_NUM_FLOATTYPE);
    offset = (imag ? type->elsize : 0);

    if (!NpyArray_ISNBO(self->descr->byteorder)) {
        NpyArray_Descr *nw;
        nw = NpyArray_DescrNew(type);
        nw->byteorder = self->descr->byteorder;
        Npy_DECREF(type);
        type = nw;
    }
    return NpyArray_NewView(type,
                            self->nd,
                            self->dimensions,
                            self->strides,
                            self, offset, NPY_FALSE);
}

NDARRAY_API NpyArray*
NpyArray_GetReal(NpyArray* self)
{
    if (NpyArray_ISCOMPLEX(self)) {
        return _get_part(self, 0);
    } else {
        Npy_INCREF(self);
        return self;
    }
}

NDARRAY_API NpyArray*
NpyArray_GetImag(NpyArray* self)
{
    if (NpyArray_ISCOMPLEX(self)) {
        return _get_part(self, 1);
    } else {
        Npy_INCREF(self);
        return self;
    }
}
