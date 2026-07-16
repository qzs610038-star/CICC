--------------------------------------------------------------------------------
-- Copyright (C) 2013-2025 Efinix Inc. All rights reserved.              
--
-- This   document  contains  proprietary information  which   is        
-- protected by  copyright. All rights  are reserved.  This notice       
-- refers to original work by Efinix, Inc. which may be derivitive       
-- of other work distributed under license of the authors.  In the       
-- case of derivative work, nothing in this notice overrides the         
-- original author's license agreement.  Where applicable, the           
-- original license agreement is included in it's original               
-- unmodified form immediately below this header.                        
--                                                                       
-- WARRANTY DISCLAIMER.                                                  
--     THE  DESIGN, CODE, OR INFORMATION ARE PROVIDED “AS IS” AND        
--     EFINIX MAKES NO WARRANTIES, EXPRESS OR IMPLIED WITH               
--     RESPECT THERETO, AND EXPRESSLY DISCLAIMS ANY IMPLIED WARRANTIES,  
--     INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF          
--     MERCHANTABILITY, NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR    
--     PURPOSE.  SOME STATES DO NOT ALLOW EXCLUSIONS OF AN IMPLIED       
--     WARRANTY, SO THIS DISCLAIMER MAY NOT APPLY TO LICENSEE.           
--                                                                       
-- LIMITATION OF LIABILITY.                                              
--     NOTWITHSTANDING ANYTHING TO THE CONTRARY, EXCEPT FOR BODILY       
--     INJURY, EFINIX SHALL NOT BE LIABLE WITH RESPECT TO ANY SUBJECT    
--     MATTER OF THIS AGREEMENT UNDER TORT, CONTRACT, STRICT LIABILITY   
--     OR ANY OTHER LEGAL OR EQUITABLE THEORY (I) FOR ANY INDIRECT,      
--     SPECIAL, INCIDENTAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES OF ANY    
--     CHARACTER INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS OF      
--     GOODWILL, DATA OR PROFIT, WORK STOPPAGE, OR COMPUTER FAILURE OR   
--     MALFUNCTION, OR IN ANY EVENT (II) FOR ANY AMOUNT IN EXCESS, IN    
--     THE AGGREGATE, OF THE FEE PAID BY LICENSEE TO EFINIX HEREUNDER    
--     (OR, IF THE FEE HAS BEEN WAIVED, $100), EVEN IF EFINIX SHALL HAVE 
--     BEEN INFORMED OF THE POSSIBILITY OF SUCH DAMAGES.  SOME STATES DO 
--     NOT ALLOW THE EXCLUSION OR LIMITATION OF INCIDENTAL OR            
--     CONSEQUENTIAL DAMAGES, SO THIS LIMITATION AND EXCLUSION MAY NOT   
--     APPLY TO LICENSEE.                                                
--
--------------------------------------------------------------------------------
------------- Begin Cut here for COMPONENT Declaration ------
component EfxSapphireHpSoc_slb is
port (
    io_peripheralClk : in std_logic;
    io_peripheralReset : in std_logic;
    io_asyncReset : out std_logic;
    io_gpio_sw_n : in std_logic;
    pll_peripheral_locked : in std_logic;
    pll_system_locked : in std_logic;
    jtagCtrl_capture : out std_logic;
    jtagCtrl_enable : out std_logic;
    jtagCtrl_reset : out std_logic;
    jtagCtrl_shift : out std_logic;
    jtagCtrl_tdi : out std_logic;
    jtagCtrl_tdo : in std_logic;
    jtagCtrl_update : out std_logic;
    ut_jtagCtrl_capture : in std_logic;
    ut_jtagCtrl_enable : in std_logic;
    ut_jtagCtrl_reset : in std_logic;
    ut_jtagCtrl_shift : in std_logic;
    ut_jtagCtrl_tdi : in std_logic;
    ut_jtagCtrl_tdo : out std_logic;
    ut_jtagCtrl_update : in std_logic;
    system_uart_0_io_rxd : in std_logic;
    system_uart_0_io_txd : out std_logic;
    cfg_done : in std_logic;
    cfg_start : out std_logic;
    cfg_sel : out std_logic;
    cfg_reset : out std_logic;
    axiAInterrupt : out std_logic;
    axiA_awaddr : in std_logic_vector(31 downto 0);
    axiA_awlen : in std_logic_vector(7 downto 0);
    axiA_awsize : in std_logic_vector(2 downto 0);
    axiA_awburst : in std_logic_vector(1 downto 0);
    axiA_awlock : in std_logic;
    axiA_awcache : in std_logic_vector(3 downto 0);
    axiA_awprot : in std_logic_vector(2 downto 0);
    axiA_awqos : in std_logic_vector(3 downto 0);
    axiA_awregion : in std_logic_vector(3 downto 0);
    axiA_awvalid : in std_logic;
    axiA_awready : out std_logic;
    axiA_wdata : in std_logic_vector(31 downto 0);
    axiA_wstrb : in std_logic_vector(3 downto 0);
    axiA_wvalid : in std_logic;
    axiA_wlast : in std_logic;
    axiA_wready : out std_logic;
    axiA_bresp : out std_logic_vector(1 downto 0);
    axiA_bvalid : out std_logic;
    axiA_bready : in std_logic;
    axiA_araddr : in std_logic_vector(31 downto 0);
    axiA_arlen : in std_logic_vector(7 downto 0);
    axiA_arsize : in std_logic_vector(2 downto 0);
    axiA_arburst : in std_logic_vector(1 downto 0);
    axiA_arlock : in std_logic;
    axiA_arcache : in std_logic_vector(3 downto 0);
    axiA_arprot : in std_logic_vector(2 downto 0);
    axiA_arqos : in std_logic_vector(3 downto 0);
    axiA_arregion : in std_logic_vector(3 downto 0);
    axiA_arvalid : in std_logic;
    axiA_arready : out std_logic;
    axiA_rdata : out std_logic_vector(31 downto 0);
    axiA_rresp : out std_logic_vector(1 downto 0);
    axiA_rlast : out std_logic;
    axiA_rvalid : out std_logic;
    axiA_rready : in std_logic;
    userInterruptA : out std_logic
);
end component EfxSapphireHpSoc_slb;

---------------------- End COMPONENT Declaration ------------
------------- Begin Cut here for INSTANTIATION Template -----
u_EfxSapphireHpSoc_slb : EfxSapphireHpSoc_slb
port map (
    io_peripheralClk => io_peripheralClk,
    io_peripheralReset => io_peripheralReset,
    io_asyncReset => io_asyncReset,
    io_gpio_sw_n => io_gpio_sw_n,
    pll_peripheral_locked => pll_peripheral_locked,
    pll_system_locked => pll_system_locked,
    jtagCtrl_capture => jtagCtrl_capture,
    jtagCtrl_enable => jtagCtrl_enable,
    jtagCtrl_reset => jtagCtrl_reset,
    jtagCtrl_shift => jtagCtrl_shift,
    jtagCtrl_tdi => jtagCtrl_tdi,
    jtagCtrl_tdo => jtagCtrl_tdo,
    jtagCtrl_update => jtagCtrl_update,
    ut_jtagCtrl_capture => ut_jtagCtrl_capture,
    ut_jtagCtrl_enable => ut_jtagCtrl_enable,
    ut_jtagCtrl_reset => ut_jtagCtrl_reset,
    ut_jtagCtrl_shift => ut_jtagCtrl_shift,
    ut_jtagCtrl_tdi => ut_jtagCtrl_tdi,
    ut_jtagCtrl_tdo => ut_jtagCtrl_tdo,
    ut_jtagCtrl_update => ut_jtagCtrl_update,
    system_uart_0_io_rxd => system_uart_0_io_rxd,
    system_uart_0_io_txd => system_uart_0_io_txd,
    cfg_done => cfg_done,
    cfg_start => cfg_start,
    cfg_sel => cfg_sel,
    cfg_reset => cfg_reset,
    axiAInterrupt => axiAInterrupt,
    axiA_awaddr => axiA_awaddr,
    axiA_awlen => axiA_awlen,
    axiA_awsize => axiA_awsize,
    axiA_awburst => axiA_awburst,
    axiA_awlock => axiA_awlock,
    axiA_awcache => axiA_awcache,
    axiA_awprot => axiA_awprot,
    axiA_awqos => axiA_awqos,
    axiA_awregion => axiA_awregion,
    axiA_awvalid => axiA_awvalid,
    axiA_awready => axiA_awready,
    axiA_wdata => axiA_wdata,
    axiA_wstrb => axiA_wstrb,
    axiA_wvalid => axiA_wvalid,
    axiA_wlast => axiA_wlast,
    axiA_wready => axiA_wready,
    axiA_bresp => axiA_bresp,
    axiA_bvalid => axiA_bvalid,
    axiA_bready => axiA_bready,
    axiA_araddr => axiA_araddr,
    axiA_arlen => axiA_arlen,
    axiA_arsize => axiA_arsize,
    axiA_arburst => axiA_arburst,
    axiA_arlock => axiA_arlock,
    axiA_arcache => axiA_arcache,
    axiA_arprot => axiA_arprot,
    axiA_arqos => axiA_arqos,
    axiA_arregion => axiA_arregion,
    axiA_arvalid => axiA_arvalid,
    axiA_arready => axiA_arready,
    axiA_rdata => axiA_rdata,
    axiA_rresp => axiA_rresp,
    axiA_rlast => axiA_rlast,
    axiA_rvalid => axiA_rvalid,
    axiA_rready => axiA_rready,
    userInterruptA => userInterruptA
);

------------------------ End INSTANTIATION Template ---------
