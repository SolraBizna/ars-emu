.MACRO callStrout
	LDA #<\1
	STA p_strout
	LDA #>\1
	STA p_strout+1
	JSR strout
.ENDM
