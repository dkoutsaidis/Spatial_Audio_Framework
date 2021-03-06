/*
 Copyright 2016-2018 Leo McCormack
 
 Permission to use, copy, modify, and/or distribute this software for any purpose with or
 without fee is hereby granted, provided that the above copyright notice and this permission
 notice appear in all copies.
 
 THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO
 THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT
 SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR
 ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
 OR PERFORMANCE OF THIS SOFTWARE.
*/
/*
 * Filename:
 *     saf_veclib.c
 * Description:
 *     Contains a collection of useful memory allocation functions and cross-platform
 *     complex number wrappers. Optimised linear algebra routines utilising BLAS and LAPACK
 *     are also included.
 * Dependencies:
 *     Windows users only: Intel's MKL must be installed, which can be freely aquired via:
 *     https://software.intel.com/en-us/articles/free-ipsxe-tools-and-libraries
 *     Mac users only: saf_utilities will utilise Apple's Accelerate library.
 * Author, date created:
 *     Leo McCormack, 11.07.2016
 */

#include "saf_veclib.h"
#include "saf_complex.h"

#ifndef MIN
  #define MIN(a,b) (( (a) < (b) ) ? (a) : (b))
#endif
#ifndef MAX
  #define MAX(a,b) (( (a) > (b) ) ? (a) : (b))
#endif

typedef struct sort_float {
    float val;
    int idx;
}sort_float;

int cmp_asc_float(const void *a,const void *b) {
    struct sort_float *a1 = (struct sort_float*)a;
    struct sort_float *a2 = (struct sort_float*)b;
    if((*a1).val<(*a2).val)return -1;
    else if((*a1).val>(*a2).val)return 1;
    else return 0;
}

int cmp_desc_float(const void *a,const void *b) {
    struct sort_float *a1 = (struct sort_float*)a;
    struct sort_float *a2 = (struct sort_float*)b;
    if((*a1).val>(*a2).val)return -1;
    else if((*a1).val<(*a2).val)return 1;
    else return 0;
}

static void sortf
(
    float* in_vec,    /* vector[len] to be sorted */
    float* out_vec,   /* if NULL, then in_vec is sorted "in-place" */
    int* new_idices,  /* set to NULL if you don't need them */
    int len,          /* number of elements in vectors, must be consistent with the input data */
    int descendFLAG   /* !1:ascending, 1:descending */
)
{
    int i;
    struct sort_float *data;
    
    data = malloc(len*sizeof(sort_float));
    for(i=0;i<len;i++) {
        data[i].val=in_vec[i];
        data[i].idx=i;
    }
    if(descendFLAG)
        qsort(data,len,sizeof(data[0]),cmp_desc_float);
    else
        qsort(data,len,sizeof(data[0]),cmp_asc_float);
    for(i=0;i<len;i++){
        if (out_vec!=NULL)
            out_vec[i] = data[i].val;
        else
            in_vec[i] = data[i].val; /* overwrite input vector */
        if(new_idices!=NULL)
            new_idices[i] = data[i].idx;
    }
    free(data);
}

/*------------------------------ vector-vector copy (?vvcopy) -------------------------------*/

void utility_svvcopy(const float* a, const int len, float* c)
{
#if defined(__ACCELERATE__) || defined(INTEL_MKL_VERSION)
    cblas_scopy(len, a, 1, c, 1);
#else
    memcpy(c, a, len*sizeof(float));
#endif
}

void utility_cvvcopy(const float_complex* a, const int len, float_complex* c)
{
#if defined(__ACCELERATE__) || defined(INTEL_MKL_VERSION)
    cblas_ccopy(len, a, 1, c, 1);
#else
    memcpy(c, a, len*sizeof(float_complex));
#endif
}

/*------------------------- vector-vector multiplication (?vvmul) ---------------------------*/

void utility_svvmul(float* a, const float* b, const int len, float* c)
{
#ifdef __ACCELERATE__
    if(c==NULL){
        float* tmp;
        tmp=malloc(len*sizeof(float));
        vDSP_vmul(a, 1, b, 1, tmp, 1, len);
        utility_svvcopy(tmp, len, a);
        free(tmp);
    }
    else
        vDSP_vmul(a, 1, b, 1, c, 1, len);
#elif INTEL_MKL_VERSION
	vsMul(len, a, b, c);
#else
	int i;
	for (i = 0; i < len; i++)
		c[i] = a[i] * b[i];
#endif
}

/*--------------------------- vector-vector dot product (?vvdot) ----------------------------*/

void utility_svvdot(const float* a, const float* b, const int len, float* c)
{
#if defined(__ACCELERATE__) || defined(INTEL_MKL_VERSION)
    c[0] = cblas_sdot (len, a, 1, b, 1);
#else
    int i;
    c[0] = 0.0f;
    for(i=0; i<len; i++)
        c[0] += a[i] * b[i];
#endif
}

void utility_cvvdot(const float_complex* a, const float_complex* b, const int len, CONJ_FLAG flag, float_complex* c)
{
#if defined(__ACCELERATE__) || defined(INTEL_MKL_VERSION)
    switch(flag){
        default:
        case NO_CONJ:
            cblas_cdotu_sub(len, a, 1, b, 1, c);
            break;
        case CONJ:
            cblas_cdotc_sub(len, a, 1, b, 1, c);
            break;
    }
#else
    int i;
    c[0] = cmplxf(0.0f,0.0f);
    switch(flag){
        default:
        case NO_CONJ:
            for(i=0; i<len; i++)
                c[0] = ccaddf(c[0], ccmulf(a[i], b[i]));
            break;
        case CONJ:
            for(i=0; i<len; i++)
                c[0] = ccaddf(c[0], ccmulf(conjf(a[i]), b[i]));
            break;
    }
#endif
}

/*------------------------------ vector-scalar product (?vsmul) -----------------------------*/

void utility_svsmul(float* a, const float* s, const int len, float* c)
{
#ifdef __ACCELERATE__
    if(c==NULL)
        cblas_sscal(len, s[0], a, 1);
    else
        vDSP_vsmul(a, 1, s, c, 1, len);
#elif INTEL_MKL_VERSION
	if (c == NULL)
		cblas_sscal(len, s[0], a, 1);
	else {
		memcpy(c, a, len*sizeof(float));
		cblas_sscal(len, s[0], c, 1);
	}
#else
    int i;
    for(i=0; i<len; i++)
        c[i] = a[i] * s[0];
#endif
}

void utility_cvsmul(float_complex* a, const float_complex* s, const int len, float_complex* c)
{
#if defined(__ACCELERATE__) || defined(INTEL_MKL_VERSION)
	if (c == NULL)
		cblas_cscal(len, s, a, 1);
	else {
		int i;
		for (i = 0; i<len; i++)
			c[i] = ccmulf(a[i], s[0]);
	}
#else
    int i;
    for(i=0; i<len; i++)
        c[i] = ccmulf(a[i], s[0]);
#endif
}

/*----------------------------- vector-scalar division (?vsdiv) -----------------------------*/

void utility_svsdiv(float* a, const float* s, const int len, float* c)
{
    if(s[0] == 0.0f){
        memset(c, 0, len*sizeof(float));
        return;
    }
#ifdef __ACCELERATE__
    vDSP_vsdiv(a, 1, s, c, 1, len);
#else
    int i;
    for(i=0; i<len; i++)
        c[i] = a[i] / s[0];
#endif
}

/*----------------------------- vector-scalar addition (?vsadd) -----------------------------*/

void utility_svsadd(float* a, const float* s, const int len, float* c)
{
#ifdef __ACCELERATE__
    vDSP_vsadd(a, 1, s, c, 1, len);
#else
    int i;
    for(i=0; i<len; i++)
        c[i] = a[i] + s[0];
#endif
}

/*---------------------------- vector-scalar subtraction (?vssub) ---------------------------*/

void utility_svssub(float* a, const float* s, const int len, float* c)
{
    int i;
    for(i=0; i<len; i++)
        c[i] = a[i] - s[0];
}

/*---------------------------- singular-value decomposition (?svd) --------------------------*/

void utility_ssvd(const float* A, const int dim1, const int dim2, float** U, float** S, float** V)
{
    int i, j, m, n, lda, ldu, ldvt, info, lwork;
    m = dim1; n = dim2; lda = dim1; ldu = dim1; ldvt = dim2;
    float wkopt;
    float* a, *s, *u, *vt, *work;

    a = malloc(lda*n*sizeof(float));
    s = malloc(MIN(n,m)*sizeof(float));
    u = malloc(ldu*m*sizeof(float));
    vt = malloc(ldvt*n*sizeof(float));
    /* store in column major order */
    for(i=0; i<dim1; i++)
        for(j=0; j<dim2; j++)
            a[j*dim1+i] = A[i*dim2 +j];
    lwork = -1;
    sgesvd_( "A", "A", &m, &n, a, &lda, s, u, &ldu, vt, &ldvt, &wkopt, &lwork,
           &info );
    lwork = (int)wkopt;
    work = (float*)malloc( lwork*sizeof(float) );
    sgesvd_( "A", "A", &m, &n, a, &lda, s, u, &ldu, vt, &ldvt, work, &lwork,
           &info );
    free(*U);
    free(*S);
    free(*V);
    if( info > 0 ) {
        /* svd failed to converge */
        (*U) = (*S) = (*V) = NULL;
    }
    else {
        /* svd successful */
        (*U) = malloc(dim1*dim1*sizeof(float));
        (*S) = calloc(dim1*dim2,sizeof(float));
        (*V) = malloc(dim2*dim2*sizeof(float));
        /*return as row-major*/
        for(i=0; i<dim1; i++)
            for(j=0; j<dim1; j++)
                (*U)[i*dim1+j] = u[j*dim1+i];
        /* singular values on the diagonal MIN(dim1, dim2). The remaining elements are 0.  */
        for(i=0; i<MIN(dim1, dim2); i++)
            (*S)[i*dim2+i] = s[i];
        /* lapack returns VT, i.e. row-major V already */
        for(i=0; i<dim2; i++)
            for(j=0; j<dim2; j++)
                (*V)[i*dim2+j] = vt[i*dim2+j];
    }
    
    free( (void*)a );
    free( (void*)s );
    free( (void*)u );
    free( (void*)vt );
    free( (void*)work );
}

/*------------------------ symmetric eigenvalue decomposition (?seig) -----------------------*/

void utility_sseig(const float* A, const int dim, int sortDecFLAG, float* V, float* D)
{
    int i, j, n, lda, info, lwork;
    float wkopt;
    float* work, *w, *a;
    
    n = dim;
    lda = dim;
    w = malloc(dim*sizeof(float));
    a = malloc(dim*dim*sizeof(float));
    
    /* store in column major order (i.e. transpose) */
    for(i=0; i<dim; i++)
        for(j=0; j<dim; j++)
            a[i*dim+j] = A[j*dim+i];
    
    /* solve the eigenproblem */
    lwork = -1;
    ssyev_( "Vectors", "Upper", &n, a, &lda, w, &wkopt, &lwork, &info );
    lwork = (int)wkopt;
    work = (float*)malloc( lwork*sizeof(float) );
    ssyev_( "Vectors", "Upper", &n, a, &lda, w, work, &lwork, &info );

    /* output */
    memset(D, 0, dim*dim*sizeof(float));
    if( info > 0 ) {
        /* failed to converge and find the eigenvalues */
        memset(V, 0, dim*dim*sizeof(float));
    }
    else{
        if(sortDecFLAG){
            for(i=0; i<dim; i++){
                for(j=0; j<dim; j++)
                    V[i*dim+j] = a[(dim-j-1)*dim+i]; /* traspose, back to row-major and reverse order */
                D[i*dim+i] = w[dim-i-1]; /* store along the diagonal, reversing the order */
            }
        }
        else{
            for(i=0; i<dim; i++){
                for(j=0; j<dim; j++)
                    V[i*dim+j] = a[j*dim+i]; /* traspose, back to row-major */
                D[i*dim+i] = w[i]; /* store along the diagonal */
            }
        }
    }
    
    free(w);
    free(a);
    free(work);
}

/*-----------------------------  eigenvalue decomposition (?eig) ----------------------------*/

void utility_ceig(const float_complex* A, const int dim, int sortDecFLAG, float_complex* VL, float_complex* VR, float_complex* D)
{
    int i, j, n, lda, ldvl, ldvr, info, lwork;
    float_complex wkopt;
    float_complex* work, *w, *vl, *vr, *a;
    float* rwork;
    
    n = lda = ldvl = ldvr = dim;
    rwork = malloc(2*dim*sizeof(float));
    w = malloc(dim*sizeof(float_complex));
    vl = malloc(dim*dim*sizeof(float_complex));
    vr = malloc(dim*dim*sizeof(float_complex));
    a = malloc(dim*dim*sizeof(float_complex));
    
    /* store in column major order (i.e. transpose) */
    for(i=0; i<dim; i++)
        for(j=0; j<dim; j++)
            a[i*dim+j] = A[j*dim+i];
    
    /* solve the eigenproblem */
    lwork = -1;
#ifdef __APPLE__
    cgeev_( "Vectors", "Vectors", &n, (__CLPK_complex*)a, &lda, (__CLPK_complex*)w, (__CLPK_complex*)vl,
           &ldvl, (__CLPK_complex*)vr, &ldvr, (__CLPK_complex*)&wkopt, &lwork, rwork, &info );
#elif INTEL_MKL_VERSION
    cgeev_( "Vectors", "Vectors", &n, (MKL_Complex8*)a, &lda, (MKL_Complex8*)w, (MKL_Complex8*)vl, &ldvl, (MKL_Complex8*)vr, &ldvr, (MKL_Complex8*)&wkopt, &lwork, rwork, &info );
#endif
    lwork = (int)crealf(wkopt);
    work = (float_complex*)malloc( lwork*sizeof(float_complex) );
#ifdef __APPLE__
    cgeev_( "Vectors", "Vectors", &n, (__CLPK_complex*)a, &lda, (__CLPK_complex*)w, (__CLPK_complex*)vl,
           &ldvl, (__CLPK_complex*)vr, &ldvr, (__CLPK_complex*)work, &lwork, rwork, &info );
#elif INTEL_MKL_VERSION
    cgeev_( "Vectors", "Vectors", &n, (MKL_Complex8*)a, &lda, (MKL_Complex8*)w, (MKL_Complex8*)vl, &ldvl, (MKL_Complex8*)vr, &ldvr, (MKL_Complex8*)work, &lwork, rwork, &info );
#endif
    
    /* sort the eigenvalues */
    float* wr;
    int* sort_idx;
    wr=malloc(dim*sizeof(float));
    sort_idx=malloc(dim*sizeof(int));
    for(i=0; i<dim; i++)
        wr[i] = crealf(w[i]);
    sortf(wr, NULL, sort_idx, dim, sortDecFLAG);
    
    /* output */
    if(D!=NULL)
        memset(D, 0, dim*dim*sizeof(float_complex));
    if( info > 0 ) {
        /* failed to converge and find the eigenvalues */
        if(VL!=NULL)
            memset(VL, 0, dim*dim*sizeof(float_complex));
        if(VR!=NULL)
            memset(VR, 0, dim*dim*sizeof(float_complex));
    }
    else{
        for(i=0; i<dim; i++){
            if(VL!=NULL)
                for(j=0; j<dim; j++)
                    VL[i*dim+j] = vl[sort_idx[j]*dim+i]; /* transpose, back to row-major */
            if(VR!=NULL)
                for(j=0; j<dim; j++)
                    VR[i*dim+j] = vr[sort_idx[j]*dim+i]; /* transpose, back to row-major */
            if(D!=NULL)
                D[i*dim+i] = cmplxf(wr[i], 0.0f); /* store along the diagonal */
        }
    }
    
    free(rwork);
    free(work);
    free(w);
    free(vl);
    free(vr);
    free(a);
    free(wr);
    free(sort_idx);
}

/*------------------------------ general linear solver (?glslv) -----------------------------*/

void utility_sglslv(const float* A, const int dim, float* B, int nCol, float* X)
{
    int i, j, n = dim, nrhs = nCol, lda = dim, ldb = dim, info;
    int* IPIV;
    IPIV = malloc(dim*sizeof(int));
    float* a, *b;
    a = malloc(dim*dim*sizeof(float));
    b = malloc(dim*nrhs*sizeof(float));
    
    /* store in column major order */
    for(i=0; i<dim; i++)
        for(j=0; j<dim; j++)
            a[j*dim+i] = A[i*dim+j];
    for(i=0; i<dim; i++)
        for(j=0; j<nCol; j++)
            b[j*dim+i] = B[i*nCol+j];
    
    /* solve Ax = b for each column in b (b is replaced by the solution: x) */
    sgesv_( &n, &nrhs, a, &lda, IPIV, b, &ldb, &info );
    
    if(info>0){
        /* A is singular, solution not possible */
        memset(X, 0, dim*nCol*sizeof(float));
    }
    else{
        /* store solution in row-major order */
        for(i=0; i<dim; i++)
            for(j=0; j<nCol; j++)
                X[i*nCol+j] = b[j*dim+i];
    }
    
    free(IPIV);
    free(a);
    free(b);
}

void utility_cglslv(const float_complex* A, const int dim, float_complex* B, int nCol, float_complex* X)
{
    int i, j, n = dim, nrhs = nCol, lda = dim, ldb = dim, info;
    int* IPIV;
    IPIV = malloc(dim*sizeof(int));
    float_complex* a, *b;
    a = malloc(dim*dim*sizeof(float_complex));
    b = malloc(dim*nrhs*sizeof(float_complex));
    
    /* store in column major order */
    for(i=0; i<dim; i++)
        for(j=0; j<dim; j++)
            a[j*dim+i] = A[i*dim+j];
    for(i=0; i<dim; i++)
        for(j=0; j<nCol; j++)
            b[j*dim+i] = B[i*nCol+j];
    
    /* solve Ax = b for each column in b (b is replaced by the solution: x) */
#ifdef __APPLE__
    cgesv_( &n, &nrhs, (__CLPK_complex*)a, &lda, IPIV, (__CLPK_complex*)b, &ldb, &info );
#elif INTEL_MKL_VERSION
    cgesv_( &n, &nrhs, (MKL_Complex8*)a, &lda, IPIV, (MKL_Complex8*)b, &ldb, &info );
#endif
    
    if(info>0){
        /* A is singular, solution not possible */
        memset(X, 0, dim*nCol*sizeof(float_complex));
    }
    else{
        /* store solution in row-major order */
        for(i=0; i<dim; i++)
            for(j=0; j<nCol; j++)
                X[i*nCol+j] = b[j*dim+i];
    }
    
    free(IPIV);
    free(a);
    free(b);
}

/*----------------------------- symmetric linear solver (?slslv) ----------------------------*/

void utility_sslslv(const float* A, const int dim, float* B, int nCol, float* X)
{
    int i, j, n = dim, nrhs = nCol, lda = dim, ldb = dim, info;
    float* a, *b;
    a = malloc(dim*dim*sizeof(float));
    b = malloc(dim*nrhs*sizeof(float));
    
    /* store in column major order */
    for(i=0; i<dim; i++)
        for(j=0; j<dim; j++)
            a[j*dim+i] = A[i*dim+j];
    for(i=0; i<dim; i++)
        for(j=0; j<nCol; j++)
            b[j*dim+i] = B[i*nCol+j];
    
    /* solve Ax = b for each column in b (b is replaced by the solution: x) */
    sposv_( "U", &n, &nrhs, a, &lda, b, &ldb, &info );
    
    if(info>0){
        /* A is not positive definate, solution not possible */
        memset(X, 0, dim*nCol*sizeof(float));
    }
    else{
        /* store solution in row-major order */
        for(i=0; i<dim; i++)
            for(j=0; j<nCol; j++)
                X[i*nCol+j] = b[j*dim+i];
    }
    
    free(a);
    free(b);
}

void utility_cslslv(const float_complex* A, const int dim, float_complex* B, int nCol, float_complex* X)
{
    int i, j, n = dim, nrhs = nCol, lda = dim, ldb = dim, info;
    float_complex* a, *b;
    a = malloc(dim*dim*sizeof(float_complex));
    b = malloc(dim*nrhs*sizeof(float_complex));
    
    /* store in column major order */
    for(i=0; i<dim; i++)
        for(j=0; j<dim; j++)
            a[j*dim+i] = A[i*dim+j];
    for(i=0; i<dim; i++)
        for(j=0; j<nCol; j++)
            b[j*dim+i] = B[i*nCol+j];
    
    /* solve Ax = b for each column in b (b is replaced by the solution: x) */
#ifdef __APPLE__
    cposv_( "U", &n, &nrhs, (__CLPK_complex*)a, &lda, (__CLPK_complex*)b, &ldb, &info );
#elif INTEL_MKL_VERSION
    cposv_( "U", &n, &nrhs, (MKL_Complex8*)a, &lda, (MKL_Complex8*)b, &ldb, &info );
#endif
    
    if(info>0){
        /* A is not positive definate, solution not possible */
        memset(X, 0, dim*nCol*sizeof(float_complex));
    }
    else{
        /* store solution in row-major order */
        for(i=0; i<dim; i++)
            for(j=0; j<nCol; j++)
                X[i*nCol+j] = b[j*dim+i];
    }
    
    free(a);
    free(b);
}



/*----------------------------- matrix pseudo-inverse (?pinv) -------------------------------*/

void utility_spinv(const float* inM, const int dim1, const int dim2, float* outM)
{
    
    int i, j, m, n, k, lda, ldu, ldvt, lwork, info;
    float* a, *s, *u, *vt, *inva, *work;
    float ss, wkopt;
    
    m = lda = ldu = dim1;
    n = dim2;
	k = ldvt = m < n ? m : n; 
    a = malloc(m*n*sizeof(float) );
    for(i=0; i<m; i++)
        for(j=0; j<n; j++)
            a[j*m+i] = inM[i*n+j]; /* store in column major order */
    s = (float*)malloc(k*sizeof(float));
    u = (float*)malloc(ldu*k*sizeof(float));
    vt = (float*)malloc(ldvt*n*sizeof(float));
    lwork = -1;
#ifdef __APPLE__
    sgesvd_("S", "S", (__CLPK_integer*)&m, (__CLPK_integer*)&n, a, (__CLPK_integer*)&lda,
            s, u, (__CLPK_integer*)&ldu, vt, (__CLPK_integer*)&ldvt, &wkopt, (__CLPK_integer*)&lwork, (__CLPK_integer*)&info);
#else
    sgesvd_("S", "S", &m, &n, a, &lda, s, u, &ldu, vt, &ldvt, &wkopt, &lwork, &info);
#endif
    lwork = (int)wkopt;
    work = (float*)malloc(lwork*sizeof(float));
    
#ifdef __APPLE__
    sgesvd_("S", "S", (__CLPK_integer*)&m, (__CLPK_integer*)&n, a, (__CLPK_integer*)&lda,
            s, u, (__CLPK_integer*)&ldu, vt, (__CLPK_integer*)&ldvt, work, (__CLPK_integer*) &lwork, (__CLPK_integer*)&info); /* Compute SVD */
#else
    sgesvd_("S", "S", &m, &n, a, &lda, s, u, &ldu, vt, &ldvt, work, &lwork, &info ); /* Compute SVD */
#endif
    if( info > 0 ) {
        memset(outM, 0, dim1*dim2*sizeof(float));
        free((void*)a);
        free((void*)s);
        free((void*)u);
        free((void*)vt);
        free((void*)work);
        return; /*failed to converge, output 0s */
    }
    int incx=1;
    for(i=0; i<k; i++){
        if(s[i] > 1.0e-5f)
            ss=1.0f/s[i];
        else
            ss=s[i];
        cblas_sscal(m, ss, &u[i*m], incx);     
    }
    inva = (float*)malloc(n*m*sizeof(float));
    int ld_inva=n;
    cblas_sgemm( CblasColMajor, CblasTrans, CblasTrans, n, m, k, 1.0f,
                vt, ldvt,
                u, ldu, 0.0f,
                inva, ld_inva);
    for(i=0; i<m; i++)
        for(j=0; j<n; j++)
            outM[j*m+i] = inva[i*n+j]; /* return in row-major order */

    /* clean-up */
    free((void*)a);
    free((void*)s);
    free((void*)u);
    free((void*)vt);
    free((void*)inva);
    free((void*)work);
}

void utility_dpinv(const double* inM, const int dim1, const int dim2, double* outM)
{
    
    int i, j, m, n, k, lda, ldu, ldvt, lwork, info;
    double* a, *s, *u, *vt, *inva, *work;
    double ss, wkopt;
    
    m = lda = ldu = dim1;
    n = dim2;
    k = ldvt = m < n ? m : n;
    a = malloc(m*n*sizeof(double) );
    for(i=0; i<m; i++)
        for(j=0; j<n; j++)
            a[j*m+i] = inM[i*n+j]; /* store in column major order */
    s = (double*)malloc(k*sizeof(double));
    u = (double*)malloc(ldu*k*sizeof(double));
    vt = (double*)malloc(ldvt*n*sizeof(double));
    lwork = -1;
#ifdef __APPLE__
    dgesvd_("S", "S", (__CLPK_integer*)&m, (__CLPK_integer*)&n, a, (__CLPK_integer*)&lda,
            s, u, (__CLPK_integer*)&ldu, vt, (__CLPK_integer*)&ldvt, &wkopt, (__CLPK_integer*)&lwork, (__CLPK_integer*)&info);
#else
    dgesvd_("S", "S", &m, &n, a, &lda, s, u, &ldu, vt, &ldvt, &wkopt, &lwork, &info);
#endif
    lwork = (int)wkopt;
    work = (double*)malloc(lwork*sizeof(double));
    
#ifdef __APPLE__
    dgesvd_("S", "S", (__CLPK_integer*)&m, (__CLPK_integer*)&n, a, (__CLPK_integer*)&lda,
            s, u, (__CLPK_integer*)&ldu, vt, (__CLPK_integer*)&ldvt, work, (__CLPK_integer*) &lwork, (__CLPK_integer*)&info); /* Compute SVD */
#else
    dgesvd_("S", "S", &m, &n, a, &lda, s, u, &ldu, vt, &ldvt, work, &lwork, &info ); /* Compute SVD */
#endif
    if( info > 0 ) {
        memset(outM, 0, dim1*dim2*sizeof(float));
        free((void*)a);
        free((void*)s);
        free((void*)u);
        free((void*)vt);
        free((void*)work);
        return; /*failed to converge, output 0s */
    }
    int incx=1;
    for(i=0; i<k; i++){
        if(s[i] > 1.0e-9f)
            ss=1.0f/s[i];
        else
            ss=s[i];
        cblas_dscal(m, ss, &u[i*m], incx);
    }
    inva = (double*)malloc(n*m*sizeof(double));
    int ld_inva=n;
    cblas_dgemm( CblasColMajor, CblasTrans, CblasTrans, n, m, k, 1.0f,
                vt, ldvt,
                u, ldu, 0.0f,
                inva, ld_inva);
    
    for(i=0; i<m; i++)
        for(j=0; j<n; j++)
            outM[j*m+i] = inva[i*n+j]; /* return in row-major order */
    
    /* clean-up */
    free((void*)a);
    free((void*)s);
    free((void*)u);
    free((void*)vt);
    free((void*)inva);
    free((void*)work);
}

/*-------------------------------- matrix inversion (?inv) ----------------------------------*/

void utility_sinv(float * A, const int N)
{
    int *IPIV;
    IPIV = malloc(N * sizeof(int));
    int LWORK = N*N;
    float *WORK;
    WORK = (float*)malloc(LWORK * sizeof(float));
    int INFO;
    
#ifdef __APPLE__
    sgetrf_((__CLPK_integer*)&N, (__CLPK_integer*)&N, A, (__CLPK_integer*)&N, (__CLPK_integer*)IPIV, (__CLPK_integer*)&INFO);
    sgetri_((__CLPK_integer*)&N, A, (__CLPK_integer*)&N, (__CLPK_integer*)IPIV, WORK, (__CLPK_integer*)&LWORK, (__CLPK_integer*)&INFO);
#elif INTEL_MKL_VERSION
    sgetrf_(&N, &N, A, &N, IPIV, &INFO);
    sgetri_(&N, A, &N, IPIV, WORK, &LWORK, &INFO);
#endif
    
    free(IPIV);
    free(WORK);
}

void utility_dinv(double* A, const int N)
{
    int *IPIV;
    IPIV = malloc( (N+1)*sizeof(int));
    int LWORK = N*N;
    double *WORK;
    WORK = malloc( LWORK*sizeof(double));
    int INFO;
    
#ifdef __APPLE__
    dgetrf_((__CLPK_integer*)&N, (__CLPK_integer*)&N, A, (__CLPK_integer*)&N, (__CLPK_integer*)IPIV, (__CLPK_integer*)&INFO);
    dgetri_((__CLPK_integer*)&N, A, (__CLPK_integer*)&N, (__CLPK_integer*)IPIV, WORK, (__CLPK_integer*)&LWORK, (__CLPK_integer*)&INFO);
#elif INTEL_MKL_VERSION
    dgetrf_(&N, &N, A, &N, IPIV, &INFO);
    dgetri_(&N, A, &N, IPIV, WORK, &LWORK, &INFO);
#endif
    
    free((void*)IPIV);
    free((void*)WORK);
}

void utility_cinv(float_complex * A, const int N)
{
    int *IPIV;
    IPIV = malloc(N * sizeof(int));
    int LWORK = N*N;
    float_complex *WORK;
    WORK = (float_complex*)malloc(LWORK * sizeof(float_complex));
    int INFO;
    
#ifdef __APPLE__
    cgetrf_((__CLPK_integer*)&N, (__CLPK_integer*)&N, (__CLPK_complex*)A, (__CLPK_integer*)&N, (__CLPK_integer*)IPIV, (__CLPK_integer*)&INFO);
    cgetri_((__CLPK_integer*)&N, (__CLPK_complex*)A, (__CLPK_integer*)&N, (__CLPK_integer*)IPIV, (__CLPK_complex*)WORK, (__CLPK_integer*)&LWORK, (__CLPK_integer*)&INFO);
#elif INTEL_MKL_VERSION
    cgetrf_(&N, &N, (MKL_Complex8*)A, &N, IPIV, &INFO);
    cgetri_(&N, (MKL_Complex8*)A, &N, IPIV, (MKL_Complex8*)WORK, &LWORK, &INFO);
#endif
    
    free(IPIV);
    free(WORK);
}



