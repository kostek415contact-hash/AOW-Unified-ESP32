## What and why / Co i dlaczego

## How was it checked / Jak sprawdzono

- [ ] `bash tools/run_tests.sh` passes (unit tests, panel JS syntax, translations) / przechodzi
- [ ] Tested on hardware (describe) / Sprawdzone na sprzęcie (opisz): 
- [ ] New texts shown to the user use translation keys (`TL('key')`, catalogs in `web/lang/`) / nowe teksty przez klucze tłumaczeń
- [ ] `loop()` still never blocks / `loop()` nadal nie blokuje
- [ ] No passwords, tokens or personal data in the change / brak haseł, tokenów i danych osobowych

> Remember: a change to firmware files triggers an automatic release. Documentation, tests and tools do not.
