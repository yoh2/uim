;;; custom.scm: Customization support
;;;
;;; Copyright (c) 2003-2005 uim Project http://uim.freedesktop.org/
;;;
;;; All rights reserved.
;;;
;;; Redistribution and use in source and binary forms, with or without
;;; modification, are permitted provided that the following conditions
;;; are met:
;;; 1. Redistributions of source code must retain the above copyright
;;;    notice, this list of conditions and the following disclaimer.
;;; 2. Redistributions in binary form must reproduce the above copyright
;;;    notice, this list of conditions and the following disclaimer in the
;;;    documentation and/or other materials provided with the distribution.
;;; 3. Neither the name of authors nor the names of its contributors
;;;    may be used to endorse or promote products derived from this software
;;;    without specific prior written permission.
;;;
;;; THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
;;; ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
;;; IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
;;; ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
;;; FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
;;; DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
;;; OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
;;; HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
;;; LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
;;; OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
;;; SUCH DAMAGE.
;;;;

;; There are complex definitions to experiment the customization
;; mechanism. Will be simplified once the requirement is cleared up.
;; -- YamaKen

(require "i18n.scm")
(require "util.scm")

;; private
(define custom-rec-alist ())
(define custom-group-rec-alist ())
(define custom-subgroup-alist ())

(define custom-activity-hooks ())
(define custom-update-hooks ())
(define custom-get-hooks ())
(define custom-set-hooks ())
(define custom-literalize-hooks ())

(define custom-validator-alist
  '((boolean   . custom-boolean?)
    (integer   . custom-integer?)
    (string    . custom-string?)
    (pathname  . custom-pathname?)
    (choice    . custom-valid-choice?)
    (key       . custom-key?)))

(define anything?
  (lambda (x)
    #t))

(define custom-boolean?
  (lambda (x)
    #t))

(define custom-integer?
  (lambda (x min max)
    (and (integer? x)
	 (<= min x)
	 (<= x max))))

(define custom-string?
  (lambda (x regex)
    (string? x)))

(define custom-pathname?
  (lambda (str)
    (string? str)))

(define custom-valid-choice?
  (lambda arg
    (let* ((sym (car arg))
	   (choice-rec-alist (cdr arg)))
      (and (symbol? sym)
	   (assq sym choice-rec-alist)
	   #t))))

(define-record 'custom-choice-rec
  '((sym   #f)
    (label "")
    (desc  "")))

(define custom-choice-label
  (lambda (custom-sym val-sym)
    (let* ((sym-rec-alist (custom-type-attrs custom-sym))
	   (srec (assq val-sym sym-rec-alist))
	   (label (custom-choice-rec-label srec)))
      label)))

(define custom-choice-desc
  (lambda (custom-sym val-sym)
    (let* ((sym-rec-alist (custom-type-attrs custom-sym))
	   (srec (assq val-sym sym-rec-alist))
	   (desc (custom-choice-rec-desc srec)))
      desc)))

(define custom-key?
  (lambda (def)
    ))

(define-record 'custom-group-rec
  '((sym   #f)
    (label "")
    (desc  "")))

(define define-custom-group
  (lambda (gsym label desc)
    (let ((grec (custom-group-rec-new gsym label desc)))
      (if (not (custom-group-rec gsym))
	  (set! custom-group-rec-alist (cons grec
					     custom-group-rec-alist))))))

(define custom-group-rec
  (lambda (gsym)
    (assq gsym custom-group-rec-alist)))

;; API
(define custom-group-label
  (lambda (gsym)
    (custom-group-rec-label (custom-group-rec gsym))))

;; API
(define custom-group-desc
  (lambda (gsym)
    (custom-group-rec-desc (custom-group-rec gsym))))

;; API
(define custom-group-subgroups
  (lambda (gsym)
    (let ((groups (filter-map (lambda (pair)
				(let ((primary-grp (car pair))
				      (subgrp (cdr pair)))
				  (and (eq? gsym primary-grp)
				       subgrp)))
			      custom-subgroup-alist)))
      (reverse groups))))

;; API
(define custom-list-groups
  (lambda ()
    (let ((groups (map custom-group-rec-sym custom-group-rec-alist)))
      (reverse groups))))

;; API
(define custom-list-primary-groups
  (lambda ()
    (let ((groups (filter-map
		   (lambda (grec)
		     (let ((grp (custom-group-rec-sym grec)))
		       (and (assq grp custom-subgroup-alist)
			    grp)))
		   custom-group-rec-alist)))
      (reverse groups))))

;; API
;; #f means 'any group'
;; TODO: support "AND" expression
(define custom-collect-by-group
  (lambda (group)
    (filter-map (lambda (crec)
		  (and (or (not group)
			   (memq group (custom-rec-groups crec)))
		       (custom-rec-sym crec)))
		custom-rec-alist)))

(define custom-add-hook
  (lambda (custom-sym hook-sym proc)
    (set-symbol-value! hook-sym (cons (cons custom-sym proc)
				      (symbol-value hook-sym)))))

;; #f for custom-sym means 'any entries'
(define custom-remove-hook
  (lambda (custom-sym hook-sym)
    (let ((removed (if custom-sym
		       (alist-delete custom-sym (symbol-value hook-sym) eq?)
		       ()))
	  (removed? (if custom-sym
			(assq custom-sym (symbol-value hook-sym))
			(not (null? (symbol-value hook-sym))))))
      (set-symbol-value! hook-sym removed)
      removed?)))

(define custom-hook-procs
  (lambda (sym hook)
    (let* ((filter (lambda (pair)
		     (let ((custom (car pair))
			   (proc (cdr pair)))
		       (and (eq? sym custom)
			    proc))))
	   (procs (filter-map filter
			      hook)))
      procs)))

(define custom-call-hook-procs
  (lambda (sym hook)
    (let ((procs (custom-hook-procs sym hook)))
      (map (lambda (proc)
	     (proc))
	   procs))))

(define-record 'custom-rec
  '((sym     #f)
    (default #f)
    (groups  ())
    (type    ())
    (label   "")
    (desc    "")))

(define custom-rec
  (lambda (sym)
    (assq sym custom-rec-alist)))

(define define-custom
  (lambda (sym default groups type label desc)
    (let ((crec (custom-rec-new sym default groups type label desc))
	  (primary-grp (car groups))
	  (subgrps (cons 'main (cdr groups))))
      (if (not (custom-rec sym))
	  (set! custom-rec-alist (cons crec
				       custom-rec-alist)))
      (if (not (symbol-bound? sym))
	  (let ((default (if (symbol? default)
			     (list 'quote default)
			     default)))
	    (eval (list 'define sym default)
		  toplevel-env)))
      (for-each (lambda (subgrp)
		  (let ((registered (custom-group-subgroups primary-grp)))
		    (if (not (memq subgrp registered))
			(set! custom-subgroup-alist
			      (cons (cons primary-grp subgrp)
				    custom-subgroup-alist)))))
		subgrps))))

;; API
(define custom-valid?
  (lambda (sym val)
    (let* ((type-name (custom-type sym))
	   (type-attrs (custom-type-attrs sym))
	   (valid? (symbol-value (cdr (assq type-name
					    custom-validator-alist)))))
      (apply valid? (cons val type-attrs)))))

;; API
(define custom-value
  (lambda (sym)
    (custom-call-hook-procs sym custom-get-hooks)
    (let ((val (symbol-value sym)))
      (if (custom-valid? sym val)
	  val
	  (custom-default-value sym)))))

;; API
(define custom-set-value!
  (lambda (sym val)
    (and (custom-valid? sym val)
	 (let* ((custom-syms (custom-collect-by-group #f))
		(pre-activities (map custom-active? custom-syms)))
	   (set-symbol-value! sym val)
	   (custom-call-hook-procs sym custom-set-hooks)
	   (let ((post-activities (map custom-active? custom-syms)))
	     (for-each (lambda (another-sym pre post)
			 (if (or (eq? another-sym sym)
				 (not (eq? pre post)))
			     (custom-call-hook-procs another-sym
						     custom-update-hooks)))
		       custom-syms
		       pre-activities
		       post-activities)
	     #t)))))

(define custom-active?
  (lambda (sym)
    (let* ((procs (custom-hook-procs sym custom-activity-hooks))
	   (activities (map (lambda (proc)
			      (proc))
			    procs))
	   (active? (apply proc-and activities)))
      active?)))

;; API
(define custom-default?
  (lambda (sym)
    (equal? (symbol-value sym)
	    (custom-default-value sym))))

;; API
(define custom-default-value
  (lambda (sym)
    (custom-rec-default (custom-rec sym))))

;; API
(define custom-groups
  (lambda (sym)
    (custom-rec-groups (custom-rec sym))))

;; API
(define custom-type
  (lambda (sym)
    (car (custom-rec-type (custom-rec sym)))))

(define custom-type-attrs
  (lambda (sym)
    (let* ((crec (custom-rec sym))
	   (typedef (custom-rec-type crec)))
      (cdr typedef))))

;; API
(define custom-range
  (lambda (sym)
    (let* ((type (custom-type sym))
	   (attrs (custom-type-attrs sym)))
      (cond
       ((eq? type 'choice)
	(map custom-choice-rec-sym attrs))
       (else
	attrs)))))

;; API
(define custom-label
  (lambda (sym)
    (custom-rec-label (custom-rec sym))))

;; API
(define custom-desc
  (lambda (sym)
    (custom-rec-desc (custom-rec sym))))

;; API
(define custom-value-as-literal
  (lambda (sym)
    (let ((val (custom-value sym))
	  (type (custom-type sym))
	  (as-string (lambda (s)
		       (string-append
			"\""
			s
			"\""))))
      (cond
       ((or (eq? val #f)
	    (eq? type 'boolean))
	(if (eq? val #f)
	    "#f"
	    "#t"))
       ((eq? type 'integer)
	(digit->string val))
       ((eq? type 'string)
	(as-string val))
       ((eq? type 'pathname)
	(as-string val))
       ((eq? type 'choice)
	(string-append "'" (symbol->string val)))
       ((eq? type 'key)
	"")))))  ;; TODO

(define custom-definition-as-literal
  (lambda (sym)
    (let ((var (symbol->string sym))
	  (val (custom-value-as-literal sym))
	  (hooked (custom-call-hook-procs sym custom-literalize-hooks)))
      (if (not (null? hooked))
	  (apply string-append hooked)
	  (string-append
	   "(define " var " " val ")")))))

;; API
;; TODO: implement after uim 0.4.6 depending on scm-nested-eval
(define custom-broadcast-custom
  (lambda (sym)
    ))

;; API
;; #f means 'any group'
;; TODO: support "AND" expression
(define custom-broadcast-customs
  (lambda (group)
    (let ((custom-syms (custom-collect-by-group group)))
      (for-each custom-broadcast-custom custom-syms))))

(define custom-prop-update-custom-handler
  (lambda (context custom-sym val)
    (custom-set-value! custom-sym val)))

(define custom-register-update-cb
  (lambda (custom-sym ptr gate-func func)
    (and (custom-rec custom-sym)
	 (let ((cb (lambda () (gate-func func ptr custom-sym))))
	   (custom-add-hook custom-sym 'custom-update-hooks cb)))))
