;;; edl-mod.el --- editing EDL files with Emacs

;; Copyright (C) 2000 Jens Thoms Törring

;; Author: Jens Thoms Törring <jens@masklin.anorg.chemie.uni-frankfurt.de>
;; Maintainer: Jens Thoms Törring <jens@masklin.anorg.chemie.uni-frankfurt.de>
;; Keywords: languages

;; This file is part of GNU Emacs.

;; GNU Emacs is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; GNU Emacs is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs; see the file COPYING.  If not, write to the
;; Free Software Foundation, Inc., 59 Temple Place - Suite 330,
;; Boston, MA 02111-1307, USA.

;;; Commentary:

;; This package provides Emacs support for editing EDL files. But please note:
;; First, it's even pre-alpha (I'm at a very early stage of learning elisp),
;; and second, nearly all it does yet is taking care of highlighting EDL
;; keywords and it's not even good at it... On the other hand, for less than
;; one day of learning elisp and how to write a mode it may be good enough ;-)

;; The currently most annoying bugs are that a `"' in a commment will switch
;; off displaying the comment in comment-face and that e.g. for both `0V' and
;; `bV' the `V' will be shown with the variable-name-face, even though it
;; should only in the first case.
;;
;; To get support for EDL mode being loaded automatically for all files with
;; the extension ".els" add the following lines to your .emacs file:
;;    (autoload 'edl-mode "edl-mod")
;;    (setq auto-mode-alist (append (list (cons "\\.edl\\'" 'edl-mode))
;;                                   auto-mode-alist))
;;

;;; Code:


(defconst edl-maintainer-address
  "Jens Thoms Törring <jens@masklin.anorg.chemie.uni-frankfurt.de>"
  "Current maintainer of the Emacs EDL package.")

(defgroup edl nil
  "Major mode for editing EDL source files."
  :group 'edl)

(defcustom edl-mode-hook nil
  "*Hook to be run when EDL mode is started."
  :type 'hook
  :group 'edl)

;(setq edl-mode-syntax-table (make-syntax-table))
;(modify-syntax-entry ?_ "w" edl-mode-syntax-table)


(defvar edl-comments
  '( "\\/\\/.*"                                              ; C++ style
     "\\/\\*\\(\\([^\\*]*\\)\\|\\(\\*[^\\/]\\)*\\)\\*\\/" )) ; C style

(defvar edl-strings
  '( "\\\"[^\"]*\\\"" ))

(defvar edl-reserved-words
  '( "P\\(ULSE\\)?_?[0-9]+\\(\\.\\|\\b\\)"
     "PH\\(ASE\\)?\\(_?[12]\\)?_?S\\(ET\\)?\\(U\\(P\\)?\\)?:?\\>"
     "PH\\(ASE\\)?_?C\\(YC\\(LE\\)?\\)??\\>"
     "PH\\(ASE\\)?_?S\\(EQ\\(UENCE\\)?\\)?\\(_?[0-9]+\\)?\\>"
     "PH\\(ASE\\)?_?S\\(W\\(ITCH\\)?\\)?_?D\\(EL\\(AY\\)?\\)?\\(_?[12]\\)?:?"
     "A\\(CQ\\(UISITION\\)?\\)?_?S\\(EQ\\(UENCE\\)?\\)?\\(_?[XY]\\)?\\>"
     "M\\(ICRO\\)?_?W\\(AVE\\)?\\>"
     "T\\(RAVELING\\)?_?W\\(AVE\\)?_?T\\(UBE\\)?_?G\\(ATE\\)?\\>"
     "T\\(RAVELING\\)?_?W\\(AVE\\)?_?T\\(UBE\\)?\\>"
     "DET\\(ECTION\\)?_?G\\(ATE\\)?\\>"
     "DET\\(ECTION\\)?\\>"
     "R\\(ADIO\\)?_?F\\(REQ\\(UENCY\\)?\\)?_?G\\(ATE\\)?\\>"
     "R\\(ADIO\\)?_?F\\(REQ\\(UENCY\\)?\\)?\\>"
     "PH\\(ASE\\)?\\(_?[12]\\)?\\>"
     "O\\(THER\\)?\\(_?[1-4]\\)?\\>"
     "F\\(UNC\\(TION\\)?\\)?\\>"
     "S\\(TART\\)?\\>"
     "L\\(EN\\(GTH\\)?\\)?\\>"
     "D\\(EL\\(TA\\)?\\)?_?S\\(TART\\)?\\>"
     "D\\(EL\\(TA\\)?\\)?_?L\\(EN\\(GTH\\)?\\)?\\>"
     "M\\(AX\\(IMUM\\)?\\)?_?L\\(EN\\(GTH\\)?\\)?\\>"
     "REPL\\(ACEMENT\\)?_?\\(P\\(\\(ULSE\\)?S?\\)?\\)?\\>"
     "\\(\\(DEL\\)\\|\\(DELAY\\)\\)\\>"
     "G\\(RACE\\)?_?P\\(ERIOD\\)?:?"
     "P\\(OD\\)?_?[12]\\>"
     "P\\(OD\\)?\\>"
     "CH\\(ANNEL\\)?\\>"
     "CH[1-4]\\>"
     "MATH[1-3]\\>"
     "REF[1-4]\\>"
     "AUX[12]?\\>"
     "LIN\\>"
     "INV\\(ERT\\(ED\\)?\\)?\\>"
     "V_?H\\(IGH\\)?\\>"
     "V_?L\\(OW\\)?\\>"
     "T\\(IME\\)?_?B\\(ASE\\)?\\>"
     "T\\(RIG\\(GER\\)?\\)?_?M\\(ODE\\)?\\>"
     "MODE\\>"
     "INT\\(ERN\\(AL\\)?\\)?\\>"
     "EXT\\(ERN\\(AL\\)?\\)?\\>"
     "SL\\(OPE\\)?\\>"
     "NEG\\(ATIVE\\)?\\>"
     "POS\\(ITIVE\\)?\\>"
     "LEV\\(EL\\)?\\>"
     "REP\\(EAT\\)?_?T\\(IME\\)?\\>"
     "REP\\(EAT\\)?_?F\\(REQ\\(UENCY\\)?\\)?\\>"
     "WHILE\\>"
     "BREAK\\>"
     "NEXT\\>"
     "IF\\>"
     "ELSE\\>"
     "REPEAT\\>"
     "FOR\\>"
     "FOREVER\\>"
     "ON\\>"
     "OFF\\>"
     "[+-]?[xX]\\>"
     "[+-]?[yY]\\>"
	 "[cC][wW]\\>" ) )

(defvar edl-section-keywords
  '( "DEV\\(ICE\\)?S?:"
	 "VAR\\(IABLE\\)?S?:"
	 "ASS\\(IGNMENT\\)?S?:"
	 "PHA\\(SE\\)?S?:"
	 "PREP\\(ARATION\\)?S?:"
	 "EXP\\(ERIMENT\\)?:"
	 "ON_STOP:" ) )


(defvar edl-unit-keywords
  "\\([0-9]\\|\\<\\)\\(\\(n\\|u\\|m\\|k\\|M\\)?\\(s\\|G\\|V\\|A\\|db\\|dB\\|Hz\\)\\|\\(n\\|u\\|m\\)?T\\)\\>" )


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
   (cons "\\(<=\\|>=\\|==\\|!=\\|<\\|>\\|&\\|!\\|~\\)" 'font-lock-builtin-face)
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


;;; provide ourself

(provide 'edl-mode)

;;; edl-mod.el ends here
