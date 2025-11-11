Works in progress:
- Testing for publisher to adiak as single json string (plugin libadiak\_json.ipp).
- Publish to adiak as single fields (finish/test libadiak\_many.ipp)
- Time publish calls in demos (watch for long child processes)
- Automated doxygen publication in github c++ repo

Future work:
- Consolidate get\_temp\_file\_name into support function.
- Implement libcurl plugin
- Implement libldms plugin
- Identify compiler targets and finish implementation of IEEE sub-single float types.
- (namepath, leaf object): see get\_value; example return string of /header/application.
- add cache-and-retry wrapper plugin 
- base64 encode for preserving bit precision in float/double/complex
- additional field names for certain encodings
  - "bits": b64string
  - "endian": "little","big"

Related work:
- Submit ADC plugin to adiak, based on ADC release.
