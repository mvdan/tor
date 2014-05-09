package main

import (
	"archive/tar"
	"bufio"
	"fmt"
	"io"
	"log"
	"os/exec"
	"path"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

type Consensus struct {
	Time time.Time
	Ids  []string
	Size int64
}

type Consensuses []Consensus

type ConsensusStats struct {
	Data          Consensuses
	MeanEntrySize int64
}

func (c Consensuses) Len() int {
	return len(c)
}

func (c Consensuses) Less(i, j int) bool {
	return c[i].Time.Before(c[j].Time)
}

func (c Consensuses) Swap(i, j int) {
	c[i], c[j] = c[j], c[i]
}

const (
	dateRawForm = "2006-01-02-15-04-05"
	dateOutForm = "2006-01-02-15-04-05"
)

func analyze(cs Consensuses, tr *tar.Reader) Consensuses {
	for {
		hdr, err := tr.Next()
		if err == io.EOF {
			break
		}
		if err != nil {
			log.Fatalln(err)
		}
		if hdr.Typeflag != tar.TypeReg {
			continue
		}

		log.Println(hdr.Name)
		strTime := path.Base(hdr.Name)[:len("YYYY-MM-DD-HH-MM-SS")]
		parsedTime, err := time.Parse(dateRawForm, strTime)
		c := Consensus{Time: parsedTime, Size: hdr.Size}
		scanner := bufio.NewScanner(tr)
		for scanner.Scan() {
			text := scanner.Text()
			switch text[0] {
			case 'r':
				fields := strings.Split(text, " ")
				if len(fields) < 3 {
					continue
				}
				c.Ids = append(c.Ids, fields[2])
			default:
				continue
			}
		}
		if err := scanner.Err(); err != nil {
			log.Fatal(err)
		}
		cs = append(cs, c)
	}
	return cs
}

func results(cst ConsensusStats) {
	cs := cst.Data
	sort.Sort(cs)
	previousTime := cs[0].Time
	lastSeen := make(map[string]time.Time)
	csSize := int64(0)
	for _, c := range cs {
		log.Printf("Doing consensus from %s\n", c.Time.Format(dateOutForm))
		for _, id := range c.Ids {
			t, e := lastSeen[id]
			if e && t != previousTime {
				hoursGone := int(c.Time.Sub(t).Hours())
				if hoursGone > 0 {
					//log.Printf("%s had been gone for %d hours\n", id, hoursGone)
				}
			}
			lastSeen[id] = c.Time
		}
		previousTime = c.Time
		csSize += c.Size / int64(len(c.Ids))
	}
	cst.MeanEntrySize = csSize / int64(len(cs))
	log.Printf("Mean consensus entry size in bytes is %d\n", cst.MeanEntrySize)
}

func main() {
	tarpaths, err := filepath.Glob("consensuses-*.tar.*")
	if err != nil {
		log.Fatal(err)
	}

	cst := ConsensusStats{}

	for _, tp := range tarpaths {
		fmt.Printf("Parsing %s\n", tp)
		ext := filepath.Ext(tp)
		var catcmd string
		switch ext {
		case ".xz":
			catcmd = "xzcat"
		case ".bz2":
			catcmd = "bzcat"
		case ".gz":
			catcmd = "zcat"
		default:
			log.Printf("Unsupported format .tar%s in %s", ext, tp)
			continue
		}
		cmd := exec.Command(catcmd, tp)
		fr, err := cmd.StdoutPipe()
		if err != nil {
			log.Fatal(err)
		}
		defer fr.Close()
		if err := cmd.Start(); err != nil {
			log.Fatal(err)
		}
		tr := tar.NewReader(fr)
		cst.Data = analyze(cst.Data, tr)
	}
	results(cst)
}
