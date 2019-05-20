clc;
clear all;
close all;

load resposta_ao_degrau_ma_100_corrigido.mat;

Qde_amostras = 80;
Ts = 2;

for k=1:Qde_amostras
    tt = clock;
    
    try
    	resposta = webread(['http://rhulio.rscada.ga/api/257E6C/envio?tempinterna=' num2str(saida_temp(1,k)) '&tempexterna=' num2str(saida_tempe_ext(1,k)) '&tempcupula=' num2str(saida_temp_cupula(1,k))]);
    catch
    end
        
    while etime(clock, tt) < Ts
    end
end