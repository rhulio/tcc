clc;
clear all;
close all;

load resposta_degrau_malha_aberta_umidade_com_temp_controlada.mat
load resposta_ao_degrau_ma_100_corrigido.mat

% for i = 1:80,
%         pwmUmidade = webread(['https://sistema.rscada.ga/api/4709B5/envio?pwm=' num2str(pwm(i)) ...
%         '&umidade=' num2str(saida_umid(i)) '&temperatura=' num2str(saida_temp(i)) ...
%         '&temperaturacupula=' num2str(saida_temp_cupula(i)) '&temperaturaexterna=' num2str(saida_tempe_ext(i)) ...
%         '&temperaturareferencia=' num2str(ref_temp(i))]);
% end

for i = 81:777,
        pwmUmidade = webread(['https://sistema.rscada.ga/api/4709B5/envio?pwm=' num2str(pwm(i)) '&umidade=' num2str(saida_umid(i))]);
end