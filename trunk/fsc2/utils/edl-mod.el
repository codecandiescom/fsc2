;;; edl-mod.el --- editing EDL files with Emacs
;;
;;  $Id$
;;
;;  Copyright (C) 1999-2006 Jens Thoms Törring
;;
;;  Author: Jens Thoms Törring <jens@masklin.anorg.chemie.uni-frankfurt.de>
;;  Maintainer: Jens Thoms Törring <jens@masklin.anorg.chemie.uni-frankfurt.de>
;;  Keywords: languages
;;
;;  This file is part of fsc2.
;;
;;  Fsc2 is free software; you can redistribute it and/or modify
;;  it under the terms of the GNU General Public License as published by
;;  the Free Software Foundation; either version 2, or (at your option)
;;  any later version.
;;
;;  Fsc2 is distributed in the hope that it will be useful,
;;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;  GNU General Public License for more details.
;;
;;  You should have received a copy of the GNU General Public License
;;  along with fsc2; see the file COPYING.  If not, write to
;;  the Free Software Foundation, 59 Temple Place - Suite 330,
;;  Boston, MA 02111-1307, USA.

;;; Commentary:

;; This package provides Emacs support for editing EDL files. But please note:
;; First, it's even pre-alpha (I'm at a very early stage of learning elisp),
;; and second, nearly all it does yet is taking care of highlighting EDL
;; keywords and it's not too good at it... On the other hand, for less than
;; one day of learning elisp and how to write a mode it may be good enough ;-)

;; The currently most annoying bugs are that a `"' in a commment will switch
;; off displaying the comment in comment-face.
;;
;; To get support for EDL mode being loaded automatically for all files with
;; the extension ".edl" add the following lines to your .emacs file:
;; (autoload 'edl-mode "edl-mod.elc"
;;  "edl-mod.el -- editing EDL files with Emacs")
;; (setq auto-mode-alist (append (list (cons "\\.edl\\'" 'edl-mode))
;;                                auto-mode-alist))
;;


(defconst edl-maintainer-address
  "Jens Thoms Törring <jt@toerring.de>" )

(defgroup edl nil
  "Major mode for editing EDL source files."
  :group 'edl)

(defcustom edl-mode-hook nil
  "*Hook to be run when EDL mode is started."
  :type 'hook
  :group 'edl)


(defvar edl-comments
  '( "\\/\\/.*"                                               ; C++ style
     "\\/\\*\\(\\([^\\*]*\\)\\|\\(\\*[^\\/]\\)*\\)\\*\\/" ) ) ; C style

(defvar edl-strings
  '( "\\\"[^\"]*\\\"" ))

(defvar edl-reserved-words
  '( "P\\(ULSE\\)?_?[0-9]+\\(\\(#[0-9]+\\)?\\|\\.\\|\\b\\)"
     "PH\\(ASE\\)?\\(_?[12]\\)?_?S\\(ET\\)?\\(U\\(P\\)?\\)?\\(_[0-9]+\\)?:?[^A-Za-z0-9_]"
     "PH\\(ASE\\)?_?C\\(YC\\(LE\\)?\\)?[^A-Za-z0-9_]"
     "PH\\(ASE\\)?_?S\\(EQ\\(UENCE\\)?\\)?\\(_?[0-9]+\\)?[^A-Za-z0-9_]"
     "PH\\(ASE\\)?_?S\\(W\\(ITCH\\)?\\)?_?D\\(EL\\(AY\\)?\\)?\\(_?[12]\\)?[^A-Za-z0-9_]"
     "A\\(CQ\\(UISITION\\)?\\)?_?S\\(EQ\\(UENCE\\)?\\)?\\(_?[XY]\\)?[^A-Za-z0-9_]"
     "M\\(ICRO\\)?_?W\\(AVE\\)?\\(#[0-9]+\\)?[^A-Za-z0-9_]"
     "T\\(RAVELING\\)?_?W\\(AVE\\)?_?T\\(UBE\\)?_?G\\(ATE\\)?\\(#[0-9]+\\)?[^A-Za-z0-9_]"
     "T\\(RAVELING\\)?_?W\\(AVE\\)?_?T\\(UBE\\)?\\(#[0-9]+\\)?[^A-Za-z0-9_]"
     "DET\\(ECTION\\)?_?G\\(ATE\\)?\\(#[0-9]+\\)?[^A-Za-z0-9_]"
     "DET\\(ECTION\\)?\\(#[0-9]+\\)?[^A-Za-z0-9_]"
	 "DEF\\(ENSE\\)?\\(#[0-9]+\\)?[^A-Za-z0-9_]"
	 "P\\(ULSE\\)?_?SH\\(APE\\(R\\)?\\)?\\(#[0-9]+\\)?[^A-Za-z0-9_]"
     "R\\(ADIO\\)?_?F\\(REQ\\(UENCY\\)?\\)?_?G\\(ATE\\)?\\(#[0-9]+\\)?[^A-Za-z0-9_]"
     "R\\(ADIO\\)?_?F\\(REQ\\(UENCY\\)?\\)?\\(#[0-9]+\\)?[^A-Za-z0-9_]"
     "PH\\(ASE\\)?\\(_?[12]\\)?\\(#[0-9]+\\)?[^A-Za-z0-9_]"
     "O\\(THER\\)?\\(_?[1-4]\\)?\\(#[0-9]+\\)?[^A-Za-z0-9_]"
     "F\\(UNC\\(TION\\)?\\)?_?[^A-Za-z0-9z_]"
     "S\\(TART\\)?[^A-Za-z0-9_]"
     "L\\(EN\\(GTH\\)?\\)?[^A-Za-z0-9_]"
     "D\\(EL\\(TA\\)?\\)?_?S\\(TART\\)?[^A-Za-z0-9_]"
     "D\\(EL\\(TA\\)?\\)?_?L\\(EN\\(GTH\\)?\\)?[^A-Za-z0-9_]"
     "M\\(AX\\(IMUM\\)?\\)?_?L\\(EN\\(GTH\\)?\\)?[^A-Za-z0-9_]"
     "DEL\\(AY\\)?[^A-Za-z0-9_]"
     "G\\(RACE\\)?_?P\\(ERIOD\\)?[^A-Za-z0-9_]"
     "P\\(OD\\)?_?[12][^A-Za-z0-9_]"
     "P\\(OD\\)?[^A-Za-z0-9_]"
     "CH\\(ANNEL\\)[^A-Za-z0-9_]"
     "CH[0-9][^A-Za-z0-9_]"
     "CH1[0-5][^A-Za-z0-9_]"
	 "SOURCE_[0-3][^A-Za-z0-9_]"
	 "DIO[1-2][^A-Za-z0-9_]"
	 "DEFAULT_SOURCE[^A-Za-z0-9_]"
	 "NEXT_GATE[^A-Za-z0-9_]"
     "MATH[1-3][^A-Za-z0-9_]"
     "REF[1-4][^A-Za-z0-9_]"
     "AUX[12]?[^A-Za-z0-9_]"
     "LIN[^A-Za-z0-9_]"
     "INV\\(ERT\\(ED\\)?\\)?[^A-Za-z0-9_]"
     "V_?H\\(IGH\\)?[^A-Za-z0-9_]"
     "V_?L\\(OW\\)?[^A-Za-z0-9_]"
     "T\\(IME\\)?_?B\\(ASE\\)?\\(#[0-9]+\\)?[^A-Za-z0-9_]"
     "T\\(RIG\\(GER\\)?\\)?_?M\\(ODE\\)?\\(#[0-9]+\\)?[^A-Za-z0-9_]"
	 "MAX\\(IMUM\\)?_?PATT\\(ERN\\)?_?LEN\\(GTH\\)?\\(#[0-9]+\\)?[^A-Za-z0-9_]"
     "MODE[^A-Za-z0-9_]"
     "INT\\(ERN\\(AL\\)?\\)?[^A-Za-z0-9_]"
     "EXT\\(ERN\\(AL\\)?\\)?[^A-Za-z0-9_]"
     "SL\\(OPE\\)?[^A-Za-z0-9_]"
     "NEG\\(ATIVE\\)?[^A-Za-z0-9_]"
     "POS\\(ITIVE\\)?[^A-Za-z0-9_]"
     "LEV\\(EL\\)?[^A-Za-z0-9_]"
     "REP\\(EAT\\)?_?T\\(IME\\)?[^A-Za-z0-9_]"
     "REP\\(EAT\\)?_?F\\(REQ\\(UENCY\\)?\\)?[^A-Za-z0-9_]"
     "WHILE[^A-Za-z0-9_]"
	 "UNTIL[^A-Za-z0-9_]"
     "BREAK[^A-Za-z0-9_]"
     "NEXT[^A-Za-z0-9_]"
     "IF[^A-Za-z0-9_]"
     "ELSE[^A-Za-z0-9_]"
     "UNLESS[^A-Za-z0-9_]"
     "REPEAT[^A-Za-z0-9_]"
     "FOR[^A-Za-z0-9_]"
     "FOREVER[^A-Za-z0-9_]"
     "ON[^A-Za-z0-9_]"
     "OFF[^A-Za-z0-9_]"
     "TTL[^A-Za-z0-9_]"
     "ECL[^A-Za-z0-9_]"
	 "AND[^A-Za-z0-9_]"
	 "OR[^A-Za-z0-9_]"
	 "NOT[^A-Za-z0-9_]"
	 "XOR[^A-Za-z0-9_]"
     "[+-]?[xX]\\>"
     "[+-]?[yY]\\>"
	 "[cC][wW][^A-Za-z0-9_]" ) )

(defvar edl-section-keywords
  '( "DEV\\(ICE\\)?S?:"
	 "VAR\\(IABLE\\)?S?:"
	 "ASS\\(IGNMENT\\)?S?:"
	 "PHA\\(SE\\)?S?:"
	 "PREP\\(ARATION\\)?S?:"
	 "EXP\\(ERIMENT\\)?:"
	 "ON_STOP:"
     "#EXIT"
     "#exit"
	 "#QUIT"
	 "#quit"
	 "#INCLUDE"
	 "#include"  ) )


(defvar edl-unit-keywords
  "[+-]?\\(\\([0-9]+\\(\\.\\([0-9]+\\)?\\)?\\)\\|\\(\\.[0-9]\\)\\)\\([eE][\\+\\-]?[0-9]+\\)?[ 	]?\\(\\([numkM]?\\(s\\|m\\|G\\|V\\|A\\|db\\|dB\\|Hz\\|T\\|K\\|C\\)\\)\\|cm^-1\\)[^A-Za-z0-9_]" )


(defvar edl-font-lock-keywords
  (list
   (cons (concat "\\(" (mapconcat 'identity edl-comments "\\|") "\\)")
		 'font-lock-comment-face )
   (cons (mapconcat 'identity edl-strings "\\|")
		 'font-lock-string-face )
   (cons (concat "\\<\\("
				 (mapconcat 'identity edl-section-keywords "\\|") "\\)")
		 'font-lock-function-name-face )
   (cons (concat "\\<\\("
				 (mapconcat 'identity edl-reserved-words "\\|") "\\)\\|{\\|}")
		 'font-lock-keyword-face)
   (cons "\\(<=\\|>=\\|==\\|!=\\|<\\|>\\|&\\||\\|!\\|~\\|?\\|:\\)" 'font-lock-builtin-face)
   (cons edl-unit-keywords 'font-lock-type-face))
  "EDL expressions to highlight." )




;;;###autoload
(defun edl-mode ()
  "Major mode for editing EDL files"

  (interactive)
  (kill-all-local-variables)

  (setq major-mode 'edl-mode)
  (setq mode-name "EDL")  

  (make-local-variable 'font-lock-defaults)
  (setq font-lock-defaults '(edl-font-lock-keywords nil nil))

  ;; this is necessary for 'comment-region'

  (setq comment-start "//" )

  (run-hooks 'edl-mode-hook) )

(provide 'edl-mode)

;;; edl-mod.el ends here
