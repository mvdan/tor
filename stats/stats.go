package main

import (
	"fmt"
	"archive/tar"
	"bufio"
	"io"
	"log"
	"os/exec"
	"path"
	"path/filepath"
	"sort"
	"strings"
	//"time"
)

type Consensus struct {
	Time string
	Ids  []string
}

type Consensuses []Consensus

func (c Consensuses) Len() int {
	return len(c)
}

func (c Consensuses) Less(i, j int) bool {
	return c[i].Time < c[j].Time
}

func (c Consensuses) Swap(i, j int) {
	c[i], c[j] = c[j], c[i]
}

var consensuses Consensuses

func analyze(tr *tar.Reader) {
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
		time := path.Base(hdr.Name)[:len("YYYY-MM-DD-HH")]
		consensus := Consensus{Time: time}
		scanner := bufio.NewScanner(tr)
		for scanner.Scan() {
			text := scanner.Text()
			switch text[0] {
			case 'r':
				fields := strings.Split(text, " ")
				if len(fields) < 3 {
					continue
				}
				consensus.Ids = append(consensus.Ids, fields[2])
			default:
				continue
			}
		}
		if err := scanner.Err(); err != nil {
			log.Fatal(err)
		}
		consensuses = append(consensuses, consensus)
	}
}

func results() {
	sort.Sort(consensuses)
	hoursGone := make(map[string]int)
	for _, consensus := range consensuses {
		idsLeft := make(map[string]bool)
		for id, _ := range hoursGone {
			idsLeft[id] = true
		}
		for _, id := range consensus.Ids {
			h, e := hoursGone[id]
			if e && h > 0 {
				log.Printf("%s had been gone for %d hours\n", id, h)
			}
			delete(idsLeft, id)
			hoursGone[id] = 0
		}
		for id, _ := range idsLeft {
			hoursGone[id]++
			//log.Printf("%s has now been gone for %d hours\n", id, hoursGone[id])
		}
	}
}

func main() {
	tarpaths, err := filepath.Glob("consensuses-*.tar.*")
	if err != nil {
		log.Fatal(err)
	}

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
		analyze(tr)
	}
	results()
}
