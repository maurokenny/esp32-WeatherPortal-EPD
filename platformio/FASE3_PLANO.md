# Fase 3 - Plano de Implementação

## Objetivo
Adicionar recursos visuais avançados, melhorias de UX e funcionalidades extras ao display de clima.

---

## 1. Melhorias no Gráfico de Temperatura
**Prioridade: Alta**

- [ ] Preencher área sob a linha (fill area)
- [ ] Linha de média de temperatura (linha pontilhada)
- [ ] Marcadores de min/max com valores
- [ ] Grade horizontal discreta
- [ ] Suavização da linha (curvas Bézier simplificadas)

## 2. Status Bar Aprimorada
**Prioridade: Alta**

- [ ] Ícone de bateria com nível visual (0-4 barras)
- [ ] Ícone de WiFi com força do sinal
- [ ] Ícone de alerta quando houver avisos meteorológicos
- [ ] Layout mais compacto e visual

## 3. Fase da Lua Precisa
**Prioridade: Média**

- [ ] Calcular fase da lua baseada na data
- [ ] Selecionar ícone correto (8 fases: new, waxing crescent, first quarter, etc.)
- [ ] Mostrar porcentagem de iluminação
- [ ] Mostrar próxima lua cheia/nova

## 4. Direção do Vento
**Prioridade: Média**

- [ ] Ícone de bússola com direção do vento
- [ ] Converter degrees para pontos cardeais (N, NE, E, SE, S, SW, W, NW)
- [ ] Mostrar velocidade e rajada (gust)

## 5. Tela de Erro Profissional
**Prioridade: Média**

- [ ] Ícone de erro (warning_icon_196x196)
- [ ] Mensagens de erro traduzidas/locais
- [ ] Códigos de erro específicos (WiFi, API, JSON, etc.)
- [ ] Botão de retry visual (para displays touch - opcional)

## 6. Suporte a Alertas Meteorológicos
**Prioridade: Baixa**

- [ ] Tela dedicada para alertas (se houver)
- [ ] Ícone de alerta na tela principal
- [ ] Cores invertidas para alertas (texto branco em fundo preto)
- [ ] Rolagem de múltiplos alertas

## 7. Indicadores Visuais
**Prioridade: Média**

- [ ] Indicador de dia/noite no ícone de clima
- [ ] Indicador de tendência de temperatura (↑ ↓ →)
- [ ] Indicador de precipitação (chuva/sneve prevista)

## 8. Otimizações
**Prioridade: Baixa**

- [ ] Reduzir flash usage (remover ícones não usados)
- [ ] Cache de ícones em RAM
- [ ] Lazy loading de fontes
- [ ] Compressão de bitmaps

## 9. Configurações por #define
**Prioridade: Baixa**

- [ ] Opção de esconder widgets específicos
- [ ] Configurar número de dias de previsão (3-7)
- [ ] Configurar horas no gráfico (12h, 24h, 48h)
- [ ] Selecionar unidades (C/F, km/h / mph, etc.)

## 10. Animações de Transição (Simuladas)
**Prioridade: Baixa**

- [ ] Efeito de "flash" invertido ao atualizar
- [ ] Progress bar durante loading
- [ ] Animação de piscar no ícone de atualização

---

## Ordem de Implementação Recomendada:

1. **Semana 1:** Gráfico aprimorado + Status bar com ícones
2. **Semana 2:** Fase da lua + Direção do vento
3. **Semana 3:** Tela de erro + Alertas
4. **Semana 4:** Otimizações e configurações

---

## Memória Estimada:
- Flash adicional: ~100KB (novos ícones e código)
- RAM adicional: ~5KB (buffers de gráfico)

## Dependências:
- Ícones adicionais da biblioteca esp32-weather-epd-assets
- Possível necessidade de fontes menores para mais densidade de info
