# Emulador IoT de Saúde · versão 2 (HL7 FHIR)

Esta pasta é uma cópia independente do projeto. A versão 1, na pasta acima, não foi alterada.

## Como funciona

Os ESP32 continuam enviando por BLE (ou USB) a mesma linha JSON curta da versão 1. O painel (`interface/painel_saude.html`) faz o papel de gateway: recebe cada leitura e monta os recursos FHIR, seguindo a ideia do guia HL7 Personal Health Device (o aparelho mede, o gateway converte).

Recursos gerados:

* **Patient**: um paciente fictício (`PACIENTE-SIMULADO-001`), sem nome real nem CPF.
* **Device**: um por aparelho, identificado pelo nome BLE (ex.: `Glicosimetro-A1B2`). O comando `STATUS` agora informa a versão do firmware e o modelo, que entram no Device.
* **Observation**: uma por medida, com o horário do computador no momento em que a leitura chegou.

Todos os recursos levam a marcação `HTEST` (dado de teste), do sistema HL7 v3 ActReason.

## Mapeamento das medidas

| Medida | Código | Unidade UCUM | Perfil FHIR |
|---|---|---|---|
| Glicose capilar | LOINC 41653-7 | mg/dL | (laboratório) |
| Temperatura corporal | LOINC 8310-5 | Cel | bodytemp |
| Pressão arterial | LOINC 85354-9, com sistólica 8480-6 e diastólica 8462-4 | mm[Hg] | bp |
| Pulso | LOINC 8867-4 | /min | heartrate |
| Peso | LOINC 29463-7 | kg | bodyweight |
| IMC | LOINC 39156-5 | kg/m2 | bmi |
| Gordura corporal por bioimpedância | LOINC 77233-5 | % | |
| Água corporal | LOINC 101684-9 | % | |
| Massa muscular (calculada) | LOINC 73964-9 | kg | |
| Impedância corporal | código local `impedancia-corporal` | Ohm | |

A impedância não tem código LOINC específico, por isso usa um código local do projeto.

A faixa que o painel já mostrava (normal, elevada, febre, sobrepeso…) vai no campo `interpretation`, com os códigos HL7 N, L, H, LU e HU.

Os endereços `https://mareia.agrosus.example/...` são provisórios. Troque pelo domínio oficial do projeto quando houver um.

## Uso

1. Grave as placas como antes, com `./gravar.sh` dentro desta pasta. Só o `STATUS` mudou no firmware.
2. Abra `interface/painel_saude.html` no Chrome ou no Edge e conecte os aparelhos (ou clique em **Simular**).
3. O card **Recurso FHIR** mostra a última Observation, o Patient ou os Device.
4. **Exportar FHIR** salva um Bundle do tipo transaction em `leituras_saude_fhir.json`.
5. **Enviar ao servidor**: informe o endereço base de um servidor FHIR e clique. O painel envia somente as observações ainda não enviadas. Patient e Device usam criação condicional, para não duplicar no servidor.

## Servidor para testes

* Local, com Docker: `docker run -p 8080:8080 hapiproject/hapi:latest` e endereço `http://localhost:8080/fhir`.
* Público: `https://hapi.fhir.org/baseR4`. Tudo o que for enviado fica visível para qualquer pessoa, então use apenas dados simulados.

O servidor precisa aceitar CORS, porque o envio parte do navegador. Se aparecer falha de conexão mesmo com o servidor no ar, abra o painel por um endereço local em vez de clicar duas vezes no arquivo: dentro de `interface`, rode `python3 -m http.server 8000` e acesse `http://localhost:8000/painel_saude.html`.

## Fora desta versão

O envio à RNDS exige credenciamento e certificado ICP-Brasil e usa perfis brasileiros próprios. A estrutura dos recursos já permite essa adaptação numa etapa futura.
