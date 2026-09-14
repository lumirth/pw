/* HEW_2006.10.05                                                           
                                                                           
                                                                           
                                                                           
                                                                           
                                                                           
                                                                           
                                                                           
                                                                           
                                                                           


*/
/************************************************************************/
/*      H8/38602 Series Include File                       Ver 2.1      */
/************************************************************************/
struct st_flash {                                       /* struct FLASH */
                union {                                 /* FLMCR1       */
                      unsigned char BYTE;               /*  Byte Access */
                      struct {                          /*  Bit  Access */
                             unsigned char    :1;       /*              */
                             unsigned char SWE:1;       /*    SWE       */
                             unsigned char ESU:1;       /*    ESU       */
                             unsigned char PSU:1;       /*    PSU       */
                             unsigned char EV :1;       /*    EV        */
                             unsigned char PV :1;       /*    PV        */
                             unsigned char E  :1;       /*    E         */
                             unsigned char P  :1;       /*    P         */
                             }      BIT;                /*              */
                      }         FLMCR1;                 /*              */
                union {                                 /* FLMCR2       */
                      unsigned char BYTE;               /*  Byte Access */
                      struct {                          /*  Bit  Access */
                             unsigned char FLER:1;      /*    FLER      */
                             }      BIT;                /*              */
                      }         FLMCR2;                 /*              */
                union {                                 /* FLPWCR       */
                      unsigned char BYTE;               /*  Byte Access */
                      struct {                          /*  Bit  Access */
                             unsigned char PDWND:1;     /*    PDWND     */
                             }      BIT;                /*              */
                      }         FLPWCR;                 /*              */
                union {                                 /* EBR1         */
                      unsigned char BYTE;               /*  Byte Access */
                      struct {                          /*  Bit  Access */
                             unsigned char    :2;       /*              */
                             unsigned char EB5:1;       /*    EB5       */
                             unsigned char EB4:1;       /*    EB4       */
                             unsigned char EB3:1;       /*    EB3       */
                             unsigned char EB2:1;       /*    EB2       */
                             unsigned char EB1:1;       /*    EB1       */
                             unsigned char EB0:1;       /*    EB0       */
                             }      BIT;                /*              */
                      }         EBR1;                   /*              */
                char            wk[7];                  /*              */
                union {                                 /* FENR         */
                      unsigned char BYTE;               /*  Byte Access */
                      struct {                          /*  Bit  Access */
                             unsigned char FLSHE:1;     /*    FLSHE     */
                             }      BIT;                /*              */
                      }         FENR;                   /*              */
};                                                      /*              */
struct st_rtc {                                         /* struct RTC   */
              union {                                   /* RTCFLG       */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit  Access */
                           unsigned char FOIFG    :1;   /*    FOIFG     */
                           unsigned char WKIFG    :1;   /*    WKIFG     */
                           unsigned char DYIFG    :1;   /*    DYIFG     */
                           unsigned char HRIFG    :1;   /*    HRIFG     */
                           unsigned char MNIFG    :1;   /*    MNIFG     */
                           unsigned char _1SEIFG  :1;   /*    1SEIFG    */
                           unsigned char _05SEIFG :1;   /*    05SEIFG   */
                           unsigned char _025SEIFG:1;   /*    025SEIFG  */
                           }      BIT;                  /*              */
                    }           RTCFLG;                 /*              */
              union {                                   /* RSECDR       */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit  Access */
                           unsigned char BSY:1;         /*    BSY       */
                           unsigned char SC1:3;         /*    SC1       */
                           unsigned char SC0:4;         /*    SC0       */
                           }      BIT;                  /*              */
                    }           RSECDR;                 /*              */
              union {                                   /* RMINDR       */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit  Access */
                           unsigned char BSY:1;         /*    BSY       */
                           unsigned char MN1:3;         /*    MN1       */
                           unsigned char MN0:4;         /*    MN0       */
                           }      BIT;                  /*              */
                    }           RMINDR;                 /*              */
              union {                                   /* RHRDR        */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit  Access */
                           unsigned char BSY:1;         /*    BSY       */
                           unsigned char    :1;         /*              */
                           unsigned char HR1:2;         /*    HR1       */
                           unsigned char HR0:4;         /*    HR0       */
                           }      BIT;                  /*              */
                    }           RHRDR;                  /*              */
              union {                                   /* RWKDR        */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit  Access */
                           unsigned char BSY:1;         /*    BSY       */
                           unsigned char    :4;         /*              */
                           unsigned char WK :3;         /*    WK        */
                           }      BIT;                  /*              */
                    }           RWKDR;                  /*              */
              union {                                   /* RTCCR1       */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit  Access */
                           unsigned char RUN:1;         /*    RUN       */
                           unsigned char HR24:1;        /*    12/24     */
                           unsigned char PM :1;         /*    PM        */
                           unsigned char RST:1;         /*    RST       */
                           unsigned char INT:1;         /*    INT       */
                           }      BIT;                  /*              */
                    }           RTCCR1;                 /*              */
              union {                                   /* RTCCR2       */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit  Access */
                           unsigned char FOIE    :1;    /*    FOIE      */
                           unsigned char WKIE    :1;    /*    WKIE      */
                           unsigned char DYIE    :1;    /*    DYIE      */
                           unsigned char HRIE    :1;    /*    HRIE      */
                           unsigned char MNIE    :1;    /*    MNIE      */
                           unsigned char _1SEIE  :1;    /*    1SEIE     */
                           unsigned char _05SEIE :1;    /*    05SEIE    */
                           unsigned char _025SEIE:1;    /*    025SEIE   */
                           }      BIT;                  /*              */
                    }           RTCCR2;                 /*              */
              char              wk;                     /*              */
              union {                                   /* RTCCSR       */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit  Access */
                           unsigned char     :1;        /*              */
                           unsigned char CKSO:3;        /*    CKSO      */
                           unsigned char CKSI:4;        /*    CKSI      */
                           }      BIT;                  /*              */
                    }           RTCCSR;                 /*              */
};                                                      /*              */
struct st_iic2 {                                        /* struct IIC2  */
               union {                                  /* ICCR1        */
                     unsigned char BYTE;                /*  Byte Access */
                     struct {                           /*  Bit  Access */
                            unsigned char ICE :1;       /*    ICE       */
                            unsigned char RCVD:1;       /*    RCVD      */
                            unsigned char MST :1;       /*    MST       */
                            unsigned char TRS :1;       /*    TRS       */
                            unsigned char CKS :4;       /*    CKS       */
                            }      BIT;                 /*              */
                     }          ICCR1;                  /*              */
               union {                                  /* ICCR2        */
                     unsigned char BYTE;                /*  Byte Access */
                     struct {                           /*  Bit  Access */
                            unsigned char BBSY  :1;     /*    BBSY      */
                            unsigned char SCP   :1;     /*    SCP       */
                            unsigned char SDAO  :1;     /*    SDAO      */
                            unsigned char SDAOP :1;     /*    SDAOP     */
                            unsigned char SCLO  :1;     /*    SCLO      */
                            unsigned char       :1;     /*              */
                            unsigned char IICRST:1;     /*    IICRST    */
                            }      BIT;                 /*              */
                     }          ICCR2;                  /*              */
               union {                                  /* ICMR         */
                     unsigned char BYTE;                /*  Byte Access */
                     struct {                           /*  Bit  Access */
                            unsigned char MLS :1;       /*    MLS       */
                            unsigned char WAIT:1;       /*    WAIT      */
                            unsigned char     :2;       /*              */
                            unsigned char BCWP:1;       /*    BCWP      */
                            unsigned char BC  :3;       /*    BC        */
                            }      BIT;                 /*              */
                     }          ICMR;                   /*              */
               union {                                  /* ICIER        */
                     unsigned char BYTE;                /*  Byte Access */
                     struct {                           /*  Bit  Access */
                            unsigned char TIE  :1;      /*    TIE       */
                            unsigned char TEIE :1;      /*    TEIE      */
                            unsigned char RIE  :1;      /*    RIE       */
                            unsigned char NAKIE:1;      /*    NAKIE     */
                            unsigned char STIE :1;      /*    STIE      */
                            unsigned char ACKE :1;      /*    ACKE      */
                            unsigned char ACKBR:1;      /*    ACKBR     */
                            unsigned char ACKBT:1;      /*    ACKBT     */
                            }      BIT;                 /*              */
                     }          ICIER;                  /*              */
               union {                                  /* ICSR         */
                     unsigned char BYTE;                /*  Byte Access */
                     struct {                           /*  Bit  Access */
                            unsigned char TDRE :1;      /*    TDRE      */
                            unsigned char TEND :1;      /*    TEND      */
                            unsigned char RDRF :1;      /*    RDRF      */
                            unsigned char NACKF:1;      /*    NACKF     */
                            unsigned char STOP :1;      /*    STOP      */
                            unsigned char ALOVE:1;      /*    ALOVE     */
                            unsigned char AAS  :1;      /*    AAS       */
                            unsigned char ADZ  :1;      /*    ADZ       */
                            }      BIT;                 /*              */
                     }          ICSR;                   /*              */
               union {                                  /* SAR          */
                     unsigned char BYTE;                /*  Byte Access */
                     struct {                           /*  Bit  Access */
                            unsigned char SVA:7;        /*    SVA       */
                            unsigned char FS :1;        /*    FS        */
                            }      BIT;                 /*              */
                     }          SAR;                    /*              */
               unsigned char    ICDRT;                  /* ICDRT        */
               unsigned char    ICDRR;                  /* ICDRR        */
};                                                      /*              */
struct st_tb1 {                                         /* struct TB1   */
              union {                                   /* TMB1         */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit  Access */
                           unsigned char RLD:1;         /*    RLD       */
                           unsigned char STR:1;         /*    STR       */
                           unsigned char    :3;         /*              */
                           unsigned char CKS:3;         /*    CKS       */
                           }      BIT;                  /*              */
                    }           TMB1;                   /*              */
              unsigned char     TCB1;                   /* TCB1         */
};                                                      /*              */
struct st_comp {                                        /* struct COMP  */
               union {                                  /* CMCR0        */
                     unsigned char BYTE;                /*  Byte Access */
                     struct {                           /*  Bit  Access */
                            unsigned char CME :1;       /*    CME       */
                            unsigned char CMIE:1;       /*    CMIE      */
                            unsigned char CMR :1;       /*    CMR       */
                            unsigned char CMLS:1;       /*    CMLS      */
                            unsigned char CRS :4;       /*    CRS       */
                            }      BIT;                 /*              */
                     }          CMCR0;                  /*              */
               union {                                  /* CMCR1        */
                     unsigned char BYTE;                /*  Byte Access */
                     struct {                           /*  Bit  Access */
                            unsigned char CME :1;       /*    CME       */
                            unsigned char CMIE:1;       /*    CMIE      */
                            unsigned char CMR :1;       /*    CMR       */
                            unsigned char CMLS:1;       /*    CMLS      */
                            unsigned char CRS :4;       /*    CRS       */
                            }      BIT;                 /*              */
                     }          CMCR1;                  /*              */
               union {                                  /* CMDR         */
                     unsigned char BYTE;                /*  Byte Access */
                     struct {                           /*  Bit  Access */
                            unsigned char     :2;       /*              */
                            unsigned char CMF1:1;       /*    CMF1      */
                            unsigned char CMF0:1;       /*    CMF0      */
                            unsigned char     :2;       /*              */
                            unsigned char CDR1:1;       /*    CDR1      */
                            unsigned char CDR0:1;       /*    CDR0      */
                            }      BIT;                 /*              */
                     }          CMDR;                   /*              */
};                                                      /*              */
struct st_ssu {                                         /* struct SSU   */
              union {                                   /* SSCRH        */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit  Access */
                           unsigned char MSS :1;        /*    MSS       */
                           unsigned char BIDE:1;        /*    BIDE      */
                           unsigned char SOOS:1;        /*    SOOS      */
                           unsigned char SOL :1;        /*    SOL       */
                           unsigned char SOLP:1;        /*    SOLP      */
                           unsigned char SCKS:1;        /*    SCKS      */
                           unsigned char CSS :2;        /*    CSS       */
                           }      BIT;                  /*              */
                    }           SSCRH;                  /*              */
              union {                                   /* SSCRL        */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit  Access */
                           unsigned char      :1;       /*              */
                           unsigned char SSUMS:1;       /*    SSUMS     */
                           unsigned char SRES :1;       /*    SRES      */
                           unsigned char SCKOS:1;       /*    SCKOS     */
                           unsigned char CSOS :1;       /*    CSOS      */
                           }      BIT;                  /*              */
                    }           SSCRL;                  /*              */
              union {                                   /* SSMR         */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit  Access */
                           unsigned char MLS :1;        /*    MLS       */
                           unsigned char CPOS:1;        /*    CPOS      */
                           unsigned char CPHS:1;        /*    CPHS      */
                           unsigned char     :2;        /*              */
                           unsigned char CKS :3;        /*    CKS       */
                           }      BIT;                  /*              */
                    }           SSMR;                   /*              */
              union {                                   /* SSER         */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit Access  */
                           unsigned char TE   :1;       /*    TE        */
                           unsigned char RE   :1;       /*    RE        */
                           unsigned char RSSTP:1;       /*    RSSTP     */
                           unsigned char      :1;       /*              */
                           unsigned char TEIE :1;       /*    TEIE      */
                           unsigned char TIE  :1;       /*    TIE       */
                           unsigned char RIE  :1;       /*    RIE       */
                           unsigned char CEIE :1;       /*    CEIE      */
                           }      BIT;                  /*              */
                    }           SSER;                   /*              */
              union {                                   /* SSSR         */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit Access  */
                           unsigned char     :1;        /*              */
                           unsigned char ORER:1;        /*    ORER      */
                           unsigned char     :2;        /*              */
                           unsigned char TEND:1;        /*    TEND      */
                           unsigned char TDRE:1;        /*    TDRE      */
                           unsigned char RDRF:1;        /*    RDRF      */
                           unsigned char CE  :1;        /*    CE        */
                           }      BIT;                  /*              */
                    }           SSSR;                   /*              */
              unsigned char     wk1[4];                 /*              */
              unsigned char     SSRDR;                  /* SSRDR        */
              unsigned char     wk2;                    /*              */
              unsigned char     SSTDR;                  /* SSTDR        */
};                                                      /*              */
struct st_tw {                                          /* struct TW    */
             union {                                    /* TMRW         */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char CTS  :1;        /*    CTS       */
                          unsigned char      :1;        /*              */
                          unsigned char BUFEB:1;        /*    BUFEB     */
                          unsigned char BUFEA:1;        /*    BUFEA     */
                          unsigned char      :1;        /*              */
                          unsigned char PWMD :1;        /*    PWMD      */
                          unsigned char PWMC :1;        /*    PWMC      */
                          unsigned char PWMB :1;        /*    PWMB      */
                          }      BIT;                   /*              */
                   }            TMRW;                   /*              */
             union {                                    /* TCRW         */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char CCLR:1;         /*    CCLR      */
                          unsigned char CKS :3;         /*    CKS       */
                          unsigned char TOD :1;         /*    TOD       */
                          unsigned char TOC :1;         /*    TOC       */
                          unsigned char TOB :1;         /*    TOB       */
                          unsigned char TOA :1;         /*    TOA       */
                          }      BIT;                   /*              */
                   }            TCRW;                   /*              */
             union {                                    /* TIERW        */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char OVIE :1;        /*    OVIE      */
                          unsigned char      :3;        /*              */
                          unsigned char IMIED:1;        /*    IMIED     */
                          unsigned char IMIEC:1;        /*    IMIEC     */
                          unsigned char IMIEB:1;        /*    IMIEB     */
                          unsigned char IMIEA:1;        /*    IMIEA     */
                          }      BIT;                   /*              */
                   }            TIERW;                  /*              */
             union {                                    /* TSRW         */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char OVF :1;         /*    OVF       */
                          unsigned char     :3;         /*              */
                          unsigned char IMFD:1;         /*    IMFD      */
                          unsigned char IMFC:1;         /*    IMFC      */
                          unsigned char IMFB:1;         /*    IMFB      */
                          unsigned char IMFA:1;         /*    IMFA      */
                          }      BIT;                   /*              */
                   }            TSRW;                   /*              */
             union {                                    /* TIOR0        */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char    :1;          /*              */
                          unsigned char IOB:3;          /*    IOB       */
                          unsigned char    :1;          /*              */
                          unsigned char IOA:3;          /*    IOA       */
                          }      BIT;                   /*              */
                   }            TIOR0;                  /*              */
             union {                                    /* TIOR1        */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char    :1;          /*              */
                          unsigned char IOD:3;          /*    IOD       */
                          unsigned char    :1;          /*              */
                          unsigned char IOC:3;          /*    IOC       */
                          }      BIT;                   /*              */
                   }            TIOR1;                  /*              */
             unsigned int       TCNT;                   /* TCNT         */
             unsigned int       GRA;                    /* GRA          */
             unsigned int       GRB;                    /* GRB          */
             unsigned int       GRC;                    /* GRC          */
             unsigned int       GRD;                    /* GRD          */
};                                                      /*              */
struct st_aec {                                         /* struct AEC   */
              unsigned int      ECPWCR;                 /* ECPWCR       */
              unsigned int      ECPWDR;                 /* ECPWDR       */
              char              wk1[2];                 /*              */
              union {                                   /* AEGSR        */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit  Access */
                           unsigned char AHEGS :2;      /*    AHEGS     */
                           unsigned char ALEGS :2;      /*    ALEGS     */
                           unsigned char AIEGS :2;      /*    AIEGS     */
                           unsigned char ECPWME:1;      /*    ECPWME    */
                           }      BIT;                  /*              */
                    }           AEGSR;                  /*              */
              char              wk2;                    /*              */
              union {                                   /* ECCR         */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit  Access */
                           unsigned char ACKH:2;        /*    ACKH      */
                           unsigned char ACKL:2;        /*    ACKL      */
                           unsigned char PWCK:3;        /*    PWCK      */
                           }      BIT;                  /*              */
                    }           ECCR;                   /*              */
              union {                                   /* ECCSR        */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit  Access */
                           unsigned char OVH :1;        /*    OVH       */
                           unsigned char OVL :1;        /*    OVL       */
                           unsigned char     :1;        /*              */
                           unsigned char CH2 :1;        /*    CH2       */
                           unsigned char CUEH:1;        /*    CUEH      */
                           unsigned char CUEL:1;        /*    CUEL      */
                           unsigned char CRCH:1;        /*    CRCH      */
                           unsigned char CRCL:1;        /*    CRCL      */
                           }      BIT;                  /*              */
                    }           ECCSR;                  /*              */
              union {                                   /* EC           */
                    unsigned int WORD;                  /*  Word Access */
                    struct {                            /*  Byte Access */
                           unsigned char H;             /*    ECH       */
                           unsigned char L;             /*    ECL       */
                           }     BYTE;                  /*              */
                    }           EC;                     /*              */
};                                                      /*              */
struct st_sci3 {                                        /* struct SCI3  */
               union {                                  /* SPCR         */
                     unsigned char BYTE;                /*  Byte Access */
                     struct {                           /*  Bit  Access */
                            unsigned char       :3;     /*              */
                            unsigned char SPC3  :1;     /*    SPC3      */
                            unsigned char       :2;     /*              */
                            unsigned char SCINV1:1;     /*    SCINV1    */
                            unsigned char SCINV0:1;     /*    SCINV0    */
                            }      BIT;                 /*              */
                     }          SPCR;                   /*              */
               char             wk1[6];                 /*              */
               union {                                  /* SMR3         */
                     unsigned char BYTE;                /*  Byte Access */
                     struct {                           /*  Bit  Access */
                            unsigned char COM :1;       /*    COM       */
                            unsigned char CHR :1;       /*    CHR       */
                            unsigned char PE  :1;       /*    PE        */
                            unsigned char PM  :1;       /*    PM        */
                            unsigned char STOP:1;       /*    STOP      */
                            unsigned char MP  :1;       /*    MP        */
                            unsigned char CKS :2;       /*    CKS       */
                            }      BIT;                 /*              */
                     }          SMR3;                   /*              */
               unsigned char    BRR3;                   /* BRR3         */
               union {                                  /* SCR3         */
                     unsigned char BYTE;                /*  Byte Access */
                     struct {                           /*  Bit  Access */
                            unsigned char TIE :1;       /*    TIE       */
                            unsigned char RIE :1;       /*    RIE       */
                            unsigned char TE  :1;       /*    TE        */
                            unsigned char RE  :1;       /*    RE        */
                            unsigned char MPIE:1;       /*    MPIE      */
                            unsigned char TEIE:1;       /*    TEIE      */
                            unsigned char CKE :2;       /*    CKE       */
                            }      BIT;                 /*              */
                     }          SCR3;                   /*              */
               unsigned char    TDR3;                   /* TDR3         */
               union {                                  /* SSR3         */
                     unsigned char BYTE;                /*  Byte Access */
                     struct {                           /*  Bit  Access */
                            unsigned char TDRE:1;       /*    TDRE      */
                            unsigned char RDRF:1;       /*    RDRF      */
                            unsigned char OER :1;       /*    OER       */
                            unsigned char FER :1;       /*    FER       */
                            unsigned char PER :1;       /*    PER       */
                            unsigned char TEND:1;       /*    TEND      */
                            unsigned char MPBR:1;       /*    MPBR      */
                            unsigned char MPBT:1;       /*    MPBT      */
                            }      BIT;                 /*              */
                     }          SSR3;                   /*              */
               unsigned char    RDR3;                   /* RDR3         */
               char             wk2[8];                 /*              */
               union {                                  /* SEMR         */
                     unsigned char BYTE;                /*  Byte Access */
                     struct {                           /*  Bit  Access */
                            unsigned char     :4;       /*              */
                            unsigned char ABCS:1;       /*    ABCS      */
                            }      BIT;                 /*              */
                     }          SEMR;                   /*              */
               union {                                  /* IrCR         */
                     unsigned char BYTE;                /*  Byte Access */
                     struct {                           /*  Bit  Access */
                            unsigned char IrE  :1;      /*    IrE       */
                            unsigned char IrCKS:3;      /*    IrCKS     */
                            }      BIT;                 /*              */
                     }          IrCR;                   /*              */
};                                                      /*              */
struct st_wdt {                                         /* struct WDT   */
              union {                                   /* TMWD         */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit  Access */
                           unsigned char    :4;         /*              */
                           unsigned char CKS:4;         /*    CKS       */
                           }      BIT;                  /*              */
                    }           TMWD;                   /*              */
              union {                                   /* TCSRWD1      */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit  Access */
                           unsigned char B6WI  :1;      /*    B6WI      */
                           unsigned char TCWE  :1;      /*    TCWE      */
                           unsigned char B4WI  :1;      /*    B4WI      */
                           unsigned char TCSRWE:1;      /*    TCSRWE    */
                           unsigned char B2WI  :1;      /*    B2WI      */
                           unsigned char WDON  :1;      /*    WDON      */
                           unsigned char B0WI  :1;      /*    B0WI      */
                           unsigned char WRST  :1;      /*    WRST      */
                           }      BIT;                  /*              */
                    }           TCSRWD1;                /*              */
              union {                                   /* TCSRWD2      */
                    unsigned char BYTE;                 /*  Byte Access */
                    struct {                            /*  Bit  Access */
                           unsigned char OVF  :1;       /*    OVF       */
                           unsigned char B5WI :1;       /*    B5WI      */
                           unsigned char WTIT :1;       /*    WTIT      */
                           unsigned char B3WI :1;       /*    B3WI      */
                           unsigned char IEOVF:1;       /*    IEOVF     */
                           }      BIT;                  /*              */
                    }           TCSRWD2;                /*              */
              unsigned char     TCWD;                   /* TCWD         */
};                                                      /*              */
struct st_ad {                                          /* struct A/D   */
             unsigned int       ADRR;                   /* ADRR         */
             union {                                    /* AMR          */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char     :1;         /*              */
                          unsigned char TRGE:1;         /*    TRGE      */
                          unsigned char CKS :2;         /*    CKS       */
                          unsigned char CH  :4;         /*    CH        */
                          }      BIT;                   /*              */
                   }            AMR;                    /*              */
             union {                                    /* ADSR         */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char ADSF:1;         /*    ADSF      */
                          unsigned char LADS:1;         /*    LADS      */
                          }      BIT;                   /*              */
                   }            ADSR;                   /*              */
};                                                      /*              */
struct st_io {                                          /* struct IO    */
             union {                                    /* PUCR8        */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char   :3;           /*    Bit 7-5   */
                          unsigned char B4:1;           /*    Bit 4     */
                          unsigned char B3:1;           /*    Bit 3     */
                          unsigned char B2:1;           /*    Bit 2     */
                          }      BIT;                   /*              */
                   }            PUCR8;                  /*              */
             union {                                    /* PUCR9        */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char   :4;           /*    Bit 7-4   */
                          unsigned char B3:1;           /*    Bit 3     */
                          unsigned char B2:1;           /*    Bit 2     */
                          unsigned char B1:1;           /*    Bit 1     */
                          unsigned char B0:1;           /*    Bit 0     */
                          }      BIT;                   /*              */
                   }            PUCR9;                  /*              */
             unsigned char      TARGET_F088;            /* target-backed, vendor name unknown */
             char               wk1[3];                 /*              */
             union {                                    /* PODR9        */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char   :4;           /*    Bit 7-4   */
                          unsigned char B3:1;           /*    Bit 3     */
                          unsigned char B2:1;           /*    Bit 2     */
                          unsigned char B1:1;           /*    Bit 1     */
                          unsigned char B0:1;           /*    Bit 0     */
                          }      BIT;                   /*              */
                   }            PODR9;                  /*              */
             char               wk2[3891];              /*              */
             union {                                    /* PMR1         */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char       :2;       /*              */
                          unsigned char IRQAEC:1;       /*    IRQAEC    */
                          unsigned char FTC1  :1;       /*    FTC1      */
                          unsigned char AEVL  :1;       /*    AEVL      */
                          unsigned char CLKOUT:1;       /*    CLKOUT    */
                          unsigned char TMOW  :1;       /*    TMOW      */
                          unsigned char AEVH  :1;       /*    AEVH      */
                          }      BIT;                   /*              */
                   }            PMR1;                   /*              */
             char               wk3;                    /*              */
             union {                                    /* PMR3         */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char      :7;        /*              */
                          unsigned char VCref:1;        /*    VCref     */
                          }      BIT;                   /*              */
                   }            PMR3;                   /*              */
             char               wk4[7];                 /*              */
             union {                                    /* PMRB         */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char         :4;     /*              */
                          unsigned char ADTSTCHG:1;     /*    ADTSTCHG  */
                          unsigned char         :1;     /*              */
                          unsigned char IRQ1    :1;     /*    IRQ1      */
                          unsigned char IRQ0    :1;     /*    IRQ0      */
                          }      BIT;                   /*              */
                   }            PMRB;                   /*              */
             char               wk5[9];                 /*              */
             union {                                    /* PDR1         */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char   :5;           /*    Bit 7-3   */
                          unsigned char B2:1;           /*    Bit 2     */
                          unsigned char B1:1;           /*    Bit 1     */
                          unsigned char B0:1;           /*    Bit 0     */
                          }      BIT;                   /*              */
                   }            PDR1;                   /*              */
             char               wk6;                    /*              */
             union {                                    /* PDR3         */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char   :5;           /*    Bit 7-3   */
                          unsigned char B2:1;           /*    Bit 2     */
                          unsigned char B1:1;           /*    Bit 1     */
                          unsigned char B0:1;           /*    Bit 0     */
                          }      BIT;                   /*              */
                   }            PDR3;                   /*              */
             char               wk7[4];                 /*              */
             union {                                    /* PDR8         */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char   :3;           /*    Bit 7-5   */
                          unsigned char B4:1;           /*    Bit 4     */
                          unsigned char B3:1;           /*    Bit 3     */
                          unsigned char B2:1;           /*    Bit 2     */
                          }      BIT;                   /*              */
                   }            PDR8;                   /*              */
             union {                                    /* PDR9         */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char   :4;           /*    Bit 7-4   */
                          unsigned char B3:1;           /*    Bit 3     */
                          unsigned char B2:1;           /*    Bit 2     */
                          unsigned char B1:1;           /*    Bit 1     */
                          unsigned char B0:1;           /*    Bit 0     */
                          }      BIT;                   /*              */
                   }            PDR9;                   /*              */
             char               wk8;                    /*              */
             union {                                    /* PDRB         */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char   :2;           /*    Bit 7,6   */
                          unsigned char B5:1;           /*    Bit 5     */
                          unsigned char B4:1;           /*    Bit 4     */
                          unsigned char B3:1;           /*    Bit 3     */
                          unsigned char B2:1;           /*    Bit 2     */
                          unsigned char B1:1;           /*    Bit 1     */
                          unsigned char B0:1;           /*    Bit 0     */
                          }      BIT;                   /*              */
                   }            PDRB;                   /*              */
             char               wk9;                    /*              */
             union {                                    /* PUCR1        */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char   :5;           /*    Bit 7-3   */
                          unsigned char B2:1;           /*    Bit 2     */
                          unsigned char B1:1;           /*    Bit 1     */
                          unsigned char B0:1;           /*    Bit 0     */
                          }      BIT;                   /*              */
                   }            PUCR1;                  /*              */
             union {                                    /* PUCR3        */
                   unsigned char BYTE;                  /*  Byte Access */
                   struct {                             /*  Bit  Access */
                          unsigned char   :5;           /*    Bit 7-3   */
                          unsigned char B2:1;           /*    Bit 2     */
                          unsigned char B1:1;           /*    Bit 1     */
                          unsigned char B0:1;           /*    Bit 0     */
                          }      BIT;                   /*              */
                   }            PUCR3;                  /*              */
             char               wk10[2];                /*              */
             unsigned char      PCR1;                   /* PCR1         */
             char               wk11;                   /*              */
             unsigned char      PCR3;                   /* PCR3         */
             char               wk12[4];                /*              */
             unsigned char      PCR8;                   /* PCR8         */
             unsigned char      PCR9;                   /* PCR9         */
};                                                      /*              */
union un_pfcr {                                         /* union PFCR   */
              unsigned char BYTE;                       /*  Byte Access */
              struct {                                  /*  Bit  Access */
                     unsigned char      :3;             /*              */
                     unsigned char SSUS :1;             /*    SSUS      */
                     unsigned char IRQ1S:2;             /*    IRQ1S     */
                     unsigned char IRQ0S:2;             /*    IRQ0S     */
                     }      BIT;                        /*              */
};                                                      /*              */
union un_syscr1 {                                       /* union SYSCR1 */
                unsigned char BYTE;                     /*  Byte Access */
                struct {                                /*  Bit  Access */
                       unsigned char SSBY :1;           /*    SSBY      */
                       unsigned char STS  :3;           /*    STS       */
                       unsigned char LSON :1;           /*    LSON      */
                       unsigned char TMA3 :1;           /*    TMA3      */
                       unsigned char MA   :2;           /*    MA        */
                       }      BIT;                      /*              */
};                                                      /*              */
union un_syscr2 {                                       /* union SYSCR2 */
                unsigned char BYTE;                     /*  Byte Access */
                struct {                                /*  Bit  Access */
                       unsigned char      :3;           /*              */
                       unsigned char NESEL:1;           /*    NESEL     */
                       unsigned char DTON :1;           /*    DTON      */
                       unsigned char MSON :1;           /*    MSON      */
                       unsigned char SA   :2;           /*    SA        */
                       }      BIT;                      /*              */
};                                                      /*              */
union un_iegr {                                         /* union IEGR   */
              unsigned char BYTE;                       /*  Byte Access */
              struct {                                  /*  Bit  Access */
                     unsigned char NMIEG   :1;          /*    NMIEG     */
                     unsigned char         :1;          /*              */
                     unsigned char ADTRGNEG:1;          /*    ADTRGNEG  */
                     unsigned char         :3;          /*              */
                     unsigned char IEG1    :1;          /*    IEG1      */
                     unsigned char IEG0    :1;          /*    IEG0      */
                     }      BIT;                        /*              */
};                                                      /*              */
union un_ienr1 {                                        /* union IENR1  */
               unsigned char BYTE;                      /*  Byte Access */
               struct {                                 /*  Bit  Access */
                      unsigned char IENRTC:1;           /*    IENRTC    */
                      unsigned char       :4;           /*              */
                      unsigned char IENEC2:1;           /*    IENEC2    */
                      unsigned char IEN1  :1;           /*    IEN1      */
                      unsigned char IEN0  :1;           /*    IEN0      */
                      }      BIT;                       /*              */
};                                                      /*              */
union un_ienr2 {                                        /* union IENR2  */
               unsigned char BYTE;                      /*  Byte Access */
               struct {                                 /*  Bit  Access */
                      unsigned char       :1;           /*              */
                      unsigned char IENAD :1;           /*    IENAD     */
                      unsigned char       :3;           /*              */
                      unsigned char IENTB1:1;           /*    IENTB1    */
                      unsigned char       :1;           /*              */
                      unsigned char IENEC :1;           /*    IENEC     */
                      }      BIT;                       /*              */
};                                                      /*              */
union un_osccr {                                        /* union OSCCR  */
               unsigned char BYTE;                      /*  Byte Access */
               struct {                                 /*  Bit  Access */
                      unsigned char SUBSTP:1;           /*    SUBSTP    */
                      unsigned char RFCUT :1;           /*    RFCUT     */
                      unsigned char SUBSEL:1;           /*    SUBSEL    */
                      unsigned char       :3;           /*              */
                      unsigned char OSCF  :1;           /*    OSCF      */
                      }      BIT;                       /*              */
};                                                      /*              */
union un_irr1 {                                         /* union IRR1   */
              unsigned char BYTE;                       /*  Byte Access */
              struct {                                  /*  Bit  Access */
                     unsigned char       :5;            /*              */
                     unsigned char IRREC2:1;            /*    IRREC2    */
                     unsigned char IRRI1 :1;            /*    IRRI1     */
                     unsigned char IRRI0 :1;            /*    IRRI0     */
                     }      BIT;                        /*              */
};                                                      /*              */
union un_irr2 {                                         /* union IRR2   */
              unsigned char BYTE;                       /*  Byte Access */
              struct {                                  /*  Bit  Access */
                     unsigned char       :1;            /*              */
                     unsigned char IRRAD :1;            /*    IRRAD     */
                     unsigned char       :3;            /*              */
                     unsigned char IRRTB1:1;            /*    IRRTB1    */
                     unsigned char       :1;            /*              */
                     unsigned char IRREC :1;            /*    IRREC     */
                     }      BIT;                        /*              */
};                                                      /*              */
union un_ckstpr1 {                                      /* union CKSTPR1*/
                 unsigned char BYTE;                    /*  Byte Access */
                 struct {                               /*  Bit  Access */
                        unsigned char          :1;      /*              */
                        unsigned char S3CKSTP  :1;      /*    S3CKSTP   */
                        unsigned char          :1;      /*              */
                        unsigned char ADCKSTP  :1;      /*    ADCKSTP   */
                        unsigned char          :1;      /*              */
                        unsigned char TB1CKSTP :1;      /*    TB1CKSTP  */
                        unsigned char FROMCKSTP:1;      /*    FROMCKSTP */
                        unsigned char RTCCKSTP :1;      /*    RTCCKSTP  */
                        }      BIT;                     /*              */
};                                                      /*              */
union un_ckstpr2 {                                      /* union CKSTPR2*/
                 unsigned char BYTE;                    /*  Byte Access */
                 struct {                               /*  Bit  Access */
                        unsigned char          :1;      /*              */
                        unsigned char TWCKSTP  :1;      /*    TWCKSTP   */
                        unsigned char IICCKSTP :1;      /*    IICCKSTP  */
                        unsigned char SSUCKSTP :1;      /*    SSUCKSTP  */
                        unsigned char AECCKSTP :1;      /*    AECCKSTP  */
                        unsigned char WDCKSTP  :1;      /*    WDCKSTP   */
                        unsigned char COMPCKSTP:1;      /*    COMPCKSTP */
                        }      BIT;                     /*              */
};                                                      /*              */
#define FLASH   (*(volatile struct st_flash   *)0xF020) /* FLASH Address*/
#define RTC     (*(volatile struct st_rtc     *)0xF067) /* RTC   Address*/
#define IIC2    (*(volatile struct st_iic2    *)0xF078) /* IIC2  Address*/
#define TB1     (*(volatile struct st_tb1     *)0xF0D0) /* TB1   Address*/
#define COMP    (*(volatile struct st_comp    *)0xF0DC) /* COMP  Address*/
#define SSU     (*(volatile struct st_ssu     *)0xF0E0) /* SSU   Address*/
#define TW      (*(volatile struct st_tw      *)0xF0F0) /* TW    Address*/
#define AEC     (*(volatile struct st_aec     *)0xFF8C) /* AEC   Address*/
#define SCI3    (*(volatile struct st_sci3    *)0xFF91) /* SCI3  Address*/
#define WDT     (*(volatile struct st_wdt     *)0xFFB0) /* WDT   Address*/
#define AD      (*(volatile struct st_ad      *)0xFFBC) /* A/D   Address*/
#define IO      (*(volatile struct st_io      *)0xF086) /* IO    Address*/
#define PFCR    (*(volatile union  un_pfcr    *)0xF085) /* PFCR  Address*/
#define SYSCR1  (*(volatile union  un_syscr1  *)0xFFF0) /* SYSCR1Address*/
#define SYSCR2  (*(volatile union  un_syscr2  *)0xFFF1) /* SYSCR2Address*/
#define IEGR    (*(volatile union  un_iegr    *)0xFFF2) /* IEGR  Address*/
#define IENR1   (*(volatile union  un_ienr1   *)0xFFF3) /* IENR1 Address*/
#define IENR2   (*(volatile union  un_ienr2   *)0xFFF4) /* IENR2 Address*/
#define OSCCR   (*(volatile union  un_osccr   *)0xFFF5) /* OSCCR Address*/
#define IRR1    (*(volatile union  un_irr1    *)0xFFF6) /* IRR1  Address*/
#define IRR2    (*(volatile union  un_irr2    *)0xFFF7) /* IRR2  Address*/
#define CKSTPR1 (*(volatile union  un_ckstpr1 *)0xFFFA) /* TPR1  Address*/
#define CKSTPR2 (*(volatile union  un_ckstpr2 *)0xFFFB) /* TPR2  Address*/
#define TLB1    TCB1                            /* Change TLB1 --> TCB1 */
