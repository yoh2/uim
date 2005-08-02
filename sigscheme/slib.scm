(define (software-type) 'UNIX)

;;@ (scheme-implementation-type) should return the name of the scheme
;;; implementation loading this file.
(define (scheme-implementation-type) 'SigScheme)

;;@ (scheme-implementation-version) should return a string describing
;;; the version the scheme implementation loading this file.
(define (scheme-implementation-version) "0.1.0")

;;@ (implementation-vicinity) should be defined to be the pathname of
;;; the directory where any auxillary files to your Scheme
;;; implementation reside.
(define (implementation-vicinity)
  "/home/kzk/tarball/slib/")

(define library-vicinity
  (lambda () "/home/kzk/tarball/slib/"))

(define (slib:error . args)
  (display "slib:error : ")
  (print args))

;@
(define in-vicinity string-append)

(define *load-pathname* #f)

(define home-vicinity
  (lambda ()
    "/home/kzk/"))

(define (user-vicinity)
  "")

;@
(define sub-vicinity
  (lambda (vic name)
    (string-append vic name "/")))

;@
(define with-load-pathname
  (let ((exchange1
         (lambda (new)
           (let ((old *load-pathname*))
	     (set! *load-pathname* new)
             old))))
    (lambda (path thunk)
      (let* ((old (exchange1 path))
             (val (thunk)))
        (exchange1 old)
	val))))


;;@ *FEATURES* is a list of symbols naming the (SLIB) features
;;; initially supported by this implementation.
(define *features*
      '(
;	source				;can load scheme source files
;					;(SLIB:LOAD-SOURCE "filename")
;;;	compiled			;can load compiled files
					;(SLIB:LOAD-COMPILED "filename")
;	vicinity
;	srfi-59

		       ;; Scheme report features
   ;; R5RS-compliant implementations should provide all 9 features.

;	r5rs				;conforms to
	eval				;R5RS two-argument eval
;;;	values				;R5RS multiple values
;;;	dynamic-wind			;R5RS dynamic-wind
;;;	macro				;R5RS high level macros
	delay				;has DELAY and FORCE
;	multiarg-apply			;APPLY can take more than 2 args.
;;;	char-ready?
;	rev4-optional-procedures	;LIST-TAIL, STRING-COPY,
					;STRING-FILL!, and VECTOR-FILL!

      ;; These four features are optional in both R4RS and R5RS

	multiarg/and-			;/ and - can take more than 2 args.
;	rationalize
;	transcript			;TRANSCRIPT-ON and TRANSCRIPT-OFF
	with-file			;has WITH-INPUT-FROM-FILE and
					;WITH-OUTPUT-TO-FILE

;	r4rs				;conforms to

;	ieee-p1178			;conforms to

;	r3rs				;conforms to

;;;	rev2-procedures			;SUBSTRING-MOVE-LEFT!,
					;SUBSTRING-MOVE-RIGHT!,
					;SUBSTRING-FILL!,
					;STRING-NULL?, APPEND!, 1+,
					;-1+, <?, <=?, =?, >?, >=?
;	object-hash			;has OBJECT-HASH

;;	full-continuation		;not without the -call/cc switch
;	ieee-floating-point		;conforms to IEEE Standard 754-1985
					;IEEE Standard for Binary
					;Floating-Point Arithmetic.

			;; Other common features

;	srfi				;srfi-0, COND-EXPAND finds all srfi-*
;;;	sicp				;runs code from Structure and
					;Interpretation of Computer
					;Programs by Abelson and Sussman.
;	defmacro			;has Common Lisp DEFMACRO
;;;	record				;has user defined data structures
;	string-port			;has CALL-WITH-INPUT-STRING and
					;CALL-WITH-OUTPUT-STRING
;;;	sort
;	pretty-print
;	object->string
;;;	format				;Common-lisp output formatting
;;;	trace				;has macros: TRACE and UNTRACE
;;;	compiler			;has (COMPILER)
;;;	ed				;(ED) is editor
;	system				;posix (system <string>)
;	getenv				;posix (getenv <string>)
;;;	program-arguments		;returns list of strings (argv)
;;;	current-time			;returns time in seconds since 1/1/1970

		  ;; Implementation Specific features

;	promise
;	string-case
	))

;;@ Here for backward compatability
(define scheme-file-suffix
  (lambda () ".scm"))

;;@ (SLIB:LOAD-SOURCE "foo") should load "foo.scm" or with whatever
;;; suffix all the module files in SLIB have.  See feature 'SOURCE.
(define (slib:load-source f)
  (begin
    (load f)))

;;@ At this point SLIB:LOAD must be able to load SLIB files.
(define (slib:load file)
  (begin
    (define file (string-append file (scheme-file-suffix)))
    (slib:load-source file)))

;;@
(define slib:warn
  (lambda args
    (display "Warn : ")
    (print args)))

;;; Return argument
(define (identity x) x)

;;; SLIB:EVAL is single argument eval using the top-level (user) environment.
(define slib:eval
    (lambda (form)
      (eval form '())))

(print "load require.scm")
(load "/home/kzk/tarball/slib/require.scm")

(print "require multiarg-apply")
(require 'multiarg-apply)

(print "require srfi-1")
(require 'srfi-1)
